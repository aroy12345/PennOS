`filesystem.c`:
- `process_create_fileptrs()` should be called by `p_spawn`
- `process_delete_fileeptrs()` should be called by `p_kill`

`user-functions.c`:
- change `dup2` to manually swappign fds