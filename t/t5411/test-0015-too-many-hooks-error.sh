test_expect_success "setup too  many proc-receive hooks (ok, $PROTOCOL)" '
	write_script "proc-receive" <<-EOF &&
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic"
	EOF

	git -C "$upstream" config --add "hook.proc-receive.command" proc-receive &&
	cp proc-receive "$upstream/hooks/proc-receive"
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       next(A)  refs/for/main/topic(A)
test_expect_success "proc-receive: reject more than one configured hook" '
	test_must_fail git -C workbench push origin \
		HEAD:next \
		HEAD:refs/for/main/topic \
		>out 2>&1 &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	remote: # pre-receive hook
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/heads/next
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: error: only one "proc-receive" hook can be specified
	remote: # post-receive hook
	remote: post-receive< <ZERO-OID> <COMMIT-A> refs/heads/next
	To <URL/of/upstream.git>
	 * [new branch] HEAD -> next
	 ! [remote rejected] HEAD -> refs/for/main/topic (fail to run proc-receive hook)
	EOF
	test_cmp expect actual &&
	git -C "$upstream" show-ref >out &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	<COMMIT-A> refs/heads/main
	<COMMIT-A> refs/heads/next
	EOF
	test_cmp expect actual
'

# Refs of upstream : main(A)             next(A)
# Refs of workbench: main(A)  tags/v123
test_expect_success "cleanup ($PROTOCOL)" '
	git -C "$upstream" config --unset "hook.proc-receive.command" "proc-receive" &&
	git -C "$upstream" update-ref -d refs/heads/next
'
