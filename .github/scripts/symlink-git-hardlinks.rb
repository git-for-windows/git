#!/usr/bin/env ruby

install_prefix = ARGV[0]
puts install_prefix
git_binary = File.join(install_prefix, '/usr/local/git/bin/git')

[
  ['git'          , File.join(install_prefix, '/usr/local/git/bin')],
  ['../../bin/git', File.join(install_prefix, '/usr/local/git/libexec/git-core')]
].each do |link, path|
  Dir.glob(File.join(path, '*')).each do |file|
    next if file == git_binary
		puts "#{file} #{File.size(file)} == #{File.size(git_binary)}"
    next unless File.size(file) == File.size(git_binary)
    puts "Symlinking #{file}"
    puts `ln -sf #{link} #{file}`
    exit $?.exitstatus if $?.exitstatus != 0
  end
end