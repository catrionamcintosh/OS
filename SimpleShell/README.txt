Implements shell interface. 
- Executes each user command in a separate process.
- Assumes first string of line is command, all following strings are arguments
- Allow child process to run in background by specifying "&" as the last argument
- Allows for piping and redirection
- Built in Commands include:
  - pwd
  - exit
  - fg: takes integer as argument and brings background job indexed by the integer to the foreground
  - jobs: lists all jobs running in the background
  
  
