const { spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { isUtf8 } = require("buffer");

// Note that we are not using the `@actions/core` package as it is not available
// without either committing node_modules/ to the repository, or using something
// like ncc to bundle the code.

// See https://github.com/actions/toolkit/blob/%40actions/core%401.1.0/packages/core/src/command.ts#L81-L87
const escapeData = (s) => {
  return s
    .replace(/%/g, '%25')
    .replace(/\r/g, '%0D')
    .replace(/\n/g, '%0A')
}

const stringify = (value) => {
  if (typeof value === 'string') return value;
  if (Buffer.isBuffer(value) && isUtf8(value)) return value.toString('utf-8');
  return undefined;
}

const trimEOL = (buf) => {
  let l = buf.length
  if (l > 0 && buf[l - 1] === 0x0a) {
    l -= l > 1 && buf[l - 2] === 0x0d ? 2 : 1
  }
  return buf.slice(0, l)
}

const writeBufToFile = (buf, file) => {
  out = fs.createWriteStream(file)
  out.write(buf)
  out.end()
}

const logInfo = (message) => {
  process.stdout.write(`${message}${os.EOL}`);
}

const setFailed = (error) => {
  process.stdout.write(`::error::${escapeData(error.message)}${os.EOL}`);
  process.exitCode = 1;
}

const writeCommand = (file, name, value) => {
  // Unique delimiter to avoid conflicts with actual values
  let delim;
  for (let count = 0; ; count++) {
    delim = `XXXXXX${count}`;
    if (!name.includes(delim) && !value.includes(delim)) {
      break;
    }
  }

  fs.appendFileSync(file, `${name}<<${delim}${os.EOL}${value}${os.EOL}${delim}${os.EOL}`);
}

const setSecret = (value) => {
  value = stringify(value);

  // Masking a secret that is not a valid UTF-8 string or buffer is not useful
  if (value === undefined) return;

  process.stdout.write(
    value
      .split(/\r?\n/g)
      .filter(line => line.length > 0) // Cannot mask empty lines
      .map(
        value => `::add-mask::${escapeData(value)}${os.EOL}`
      )
      .join('')
  );
}

const setOutput = (name, value) => {
  value = stringify(value);
  if (value === undefined) {
    throw new Error(`Output value '${name}' is not a valid UTF-8 string or buffer`);
  }

  writeCommand(process.env.GITHUB_OUTPUT, name, value);
}

const exportVariable = (name, value) => {
  value = stringify(value);
  if (value === undefined) {
    throw new Error(`Environment variable '${name}' is not a valid UTF-8 string or buffer`);
  }

  writeCommand(process.env.GITHUB_ENV, name, value);
}

(async () => {
  const vault = process.env.INPUT_VAULT;
  const secrets = process.env.INPUT_SECRETS;
  // Parse and normalize secret mappings
  const secretMappings = secrets
    .split(/[\n,]+/)
    .map((entry) => entry.trim())
    .filter((entry) => entry)
    .map((entry) => {
      const [input, encoding, output] = entry.split(/(\S+)?>/).map((part) => part?.trim());
      return { input, encoding, output: output || `\$output:${input}` }; // Default output to $output:input if not specified
    });

  if (secretMappings.length === 0) {
    throw new Error('No secrets provided.');
  }

  // Fetch secrets from Azure Key Vault
  for (const { input: secretName, encoding, output } of secretMappings) {
    let az = spawnSync('az',
      [
        'keyvault',
        'secret',
        'show',
        '--vault-name',
        vault,
        '--name',
        secretName,
        '--query',
        'value',
        '--output',
        'tsv'
      ],
      {
        stdio: ['ignore', 'pipe', 'inherit'],
        shell: true // az is a batch script on Windows
      }
    );

    if (az.error) throw new Error(az.error, { cause: az.error });
    if (az.status !== 0) throw new Error(`az failed with status ${az.status}`);

    // az keyvault secret show --output tsv returns a buffer with trailing \n
    // (or \r\n on Windows), so we need to trim it specifically.
    let secretBuf = trimEOL(az.stdout);

    // Mask the raw secret value in logs
    setSecret(secretBuf);

    // Handle encoded values if specified
    // Sadly we cannot use the `--encoding` parameter of the `az keyvault
    // secret (show|download)` command as the former does not support it, and
    // the latter must be used with `--file` (we could use /dev/stdout on UNIX
    // but not on Windows).
    if (encoding) {
      switch (encoding.toLowerCase()) {
        case 'base64':
          secretBuf = Buffer.from(secretBuf.toString('utf-8'), 'base64');
          break;
        case 'ascii':
        case 'utf8':
        case 'utf-8':
          // No need to decode the existing buffer from the az command
          break;
        default:
            throw new Error(`Unsupported encoding: ${encoding}`);
        }

      // Mask the decoded value
      setSecret(secretBuf);
    }

    const outputType = output.startsWith('$env:')
    ? 'env'
    : output.startsWith('$output:')
      ? 'output'
      : 'file';

    switch (outputType) {
      case 'env':
        const varName = output.replace('$env:', '').trim();
        exportVariable(varName, secretBuf);
        logInfo(`Secret set as environment variable: ${varName}`);
        break;

      case 'output':
        const outputName = output.replace('$output:', '').trim();
        setOutput(outputName, secretBuf);
        logInfo(`Secret set as output variable: ${outputName}`);
        break;

      case 'file':
        const filePath = output.trim();
        fs.mkdirSync(path.dirname(filePath), { recursive: true });
        writeBufToFile(secretBuf, filePath);
        logInfo(`Secret written to file: ${filePath}`);
        break;
    }
  }
})().catch(setFailed);
