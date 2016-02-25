# Windows Process

A native node module for Windows that allows a node app to read the contents of another process' memory.

Note that utilizing this module requires the app to be running with administrator privileges.

## Building

To compile the extension for the first time, run 

```
$ npm i
$ npm run configure
$ npm run build
```

All subsequent builds only need `npm run build`

You can confirm everything built correctly by [running the test suite](#to-run-tests).

### Working With the Extension Locally

After building:

```node
$ node
> var process = require('./')
undefined
> process.GetRunningProcesses()
['[System Process]', 'System', 'smss.exe', ...]
```

### To run tests:

```
$ npm test
```

or to run test continuously 

```
$ npm test -- watch
```
