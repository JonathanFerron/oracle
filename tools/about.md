# About this folder

Purpose of the folder here.

- `oracle-completion.bash` — bash tab-completion for the `oracle` binary. Source it
  (e.g. from `~/.bashrc`); see the file's header comment for the install line. All
  option names and value lists (AI agent shorthands, UI languages) are read from the
  binary at completion time via its hidden `--oracle-complete[=WHAT]` option, so the
  script needs no updates when options/agents/languages are added.