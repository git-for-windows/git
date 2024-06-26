#
# build_option(
#	opt_name type help_string [default]
#	ALIASES alias1 [alias2 ...]]
# )
#
# If PRIMARY_OPT is not set, uses the value of any alias name
# provided, in order of precedence provides, to set PRIMARY_OPT and
# all listed aliases. Otherwise set all aliases to the value of
# PRIMARY_OPT, or the default if not set either. If the default is
# empty string or not provided, the value "OFF" is used, this is
# the behavior of option() in cmake.
#
# An alias can be an environment variable, specify ENV{ENV_VAR} in
# this case. If the type is BOOL and the found/default value is
# OFF/FALSE the environment variable will be unset.
#
# On cmake >= 3.13 precedence is given to CACHE variables, and the
# namesake normal variable is overwritten with the CACHE value. On
# earlier versions, the cache variable will be overwritten with the
# normal variable.
#
function(build_option opt type help_string default)
	set(aliases "${ARGN}")

	if(default STREQUAL ALIASES)
		if(ARGC LESS 5)
			message(FATAL_ERROR "build_option: ALIASES specified with no alias names.")
		endif()

		unset(default)
	elseif(NOT ARGV4 STREQUAL ALIASES)
		message(FATAL_ERROR "build_option: Syntax error.")
	elseif(ARGC LESS 6)
		message(FATAL_ERROR "build_option: ALIASES specified with no alias names.")
	else()
		list(REMOVE_AT aliases 0)
	endif()

	if(default STREQUAL "")
		if(NOT type STREQUAL BOOL)
			message(FATAL_ERROR "build_option: Empty or unspecified default options must be of type BOOL.")
		endif()

		set(default OFF)
	endif()

	# First find the first non-empty value in the option and
	# aliases, with the priority being the order listed.
	# If not found, use the default.
	unset(val)

	foreach(var IN LISTS opt aliases)
		if(var STREQUAL "")
			message(FATAL_ERROR "build_option: Option or alias names cannot be empty string.")
		endif()

		set(env_var "")
		if(var MATCHES "^ENV\\{")
			string(
				REGEX REPLACE "^ENV\\{([^}]+)}$"
				"\\1" env_var "${var}"
			)
		endif()

		if(NOT env_var STREQUAL "")
			set(val "$ENV{${env_var}}")
		else()
			if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
				set(val "$CACHE{${var}}")
			endif()

			if(val STREQUAL "")
				set(val "${${var}}")
			endif()
		endif()

		if(NOT val STREQUAL "")
			break()
		endif()
	endforeach()

	if(val STREQUAL "")
		set(val "${default}")
	endif()

	foreach(var IN LISTS opt aliases)
		if(var MATCHES "^ENV\\{")
			# Unset env var for bool OFF/FALSE.
			if(type STREQUAL BOOL AND NOT val)
				unset("${var}")
			else()
				set("${var}" "${val}")
			endif()
		else()
			set("${var}" "${val}" PARENT_SCOPE)
			set("${var}" "${val}" CACHE "${type}" "${help_string}" FORCE)
		endif()
	endforeach()
endfunction()

# vim:set sw=8 ts=8 noet:
