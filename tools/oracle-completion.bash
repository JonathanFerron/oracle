# oracle-completion.bash — bash tab-completion for the `oracle` binary.
#
# Install by sourcing this file, e.g. add to ~/.bashrc:
#   source /path/to/oracle/tools/oracle-completion.bash
#
# All option names, AI agent shorthand codes, and UI language codes are read
# directly from the oracle binary itself via its hidden `--oracle-complete
# [=WHAT]` option (see print_completion_list() in src/main/cmdline.c and
# print_ai_agent_shorthand_codes() in src/ui/shared/player_config.c) — this
# script hardcodes none of those lists, so adding a new option, AI agent, or
# language needs no change here.

# Resolve the oracle binary the user actually typed (a path, or something on
# $PATH). Prints nothing if it can't be found/executed.
_oracle_bin()
{ local cmd="${COMP_WORDS[0]}"
  case "$cmd" in
    */*) [[ -x "$cmd" ]] && printf '%s' "$cmd" ;;
    *)   type -P "$cmd" 2>/dev/null ;;
  esac
}

# _oracle_dump [agents|langs] — one candidate per line, straight from the binary.
_oracle_dump()
{ local bin
  bin="$(_oracle_bin)"
  [[ -n "$bin" ]] || return 1
  if [[ -n "$1" ]]; then
    "$bin" --oracle-complete="$1" 2>/dev/null
  else
    "$bin" --oracle-complete 2>/dev/null
  fi
}

_oracle_complete()
{ local cur prev opt_name value_prefix
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"

  # Attached short-option form -A<agent> (the only form -A itself supports;
  # per cmdline.c's usage text, -A never takes a space- or '='-separated
  # argument).
  if [[ "$cur" =~ ^-A(.*)$ ]]; then
    mapfile -t COMPREPLY < <(compgen -W "$(_oracle_dump agents)" -P "-A" -- "${BASH_REMATCH[1]}")
    return 0
  fi

  # bash's default COMP_WORDBREAKS splits "--ai=rand" into separate words at
  # '=' ("--ai" "=" "rand"); recover the option name from whichever side of
  # that split the cursor currently sits on. -ai/--ai/-u/-ul/--ui.lang only
  # accept their argument this way (optional_argument in getopt_long_only).
  if [[ "$prev" == "=" && "$COMP_CWORD" -ge 2 ]]; then
    opt_name="${COMP_WORDS[COMP_CWORD-2]}"
    value_prefix="$cur"
  elif [[ "$cur" == "=" ]]; then
    opt_name="$prev"
    value_prefix=""
  fi

  case "$opt_name" in
    -ai|--ai)
      mapfile -t COMPREPLY < <(compgen -W "$(_oracle_dump agents)" -- "$value_prefix")
      return 0
      ;;
    -u|-ul|--ui.lang)
      mapfile -t COMPREPLY < <(compgen -W "$(_oracle_dump langs)" -- "$value_prefix")
      return 0
      ;;
  esac
  [[ -n "$opt_name" ]] && return 0  # recognized '=' split with nothing to suggest (e.g. -p=)

  case "$prev" in
    -A|-ai|--ai|-u|-ul|--ui.lang|-p|-pr|--prng.seed)
      return 0  # these only take an attached or '='-joined argument
      ;;
    -i|-in|--input|-o|-ou|--output)
      mapfile -t COMPREPLY < <(compgen -f -- "$cur")
      return 0
      ;;
    -n|-ns|--numsim)
      return 0  # numeric argument; nothing useful to suggest
      ;;
  esac

  local opts
  opts="$(_oracle_dump)"
  [[ -n "$opts" ]] || return 0

  mapfile -t COMPREPLY < <(compgen -W "$opts" -- "$cur")
  if [[ ${#COMPREPLY[@]} -eq 1 && "${COMPREPLY[0]}" == *= ]]; then
    compopt -o nospace 2>/dev/null  # land the cursor ready for the value
  fi
}

complete -F _oracle_complete oracle ./bin/oracle bin/oracle oracle.exe
