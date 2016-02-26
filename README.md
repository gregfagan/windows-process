# Windows Process

A native node module for Windows that allows a node app to read the contents of another process' memory.

Note that utilizing this module requires the app to be running with administrator privileges.

## API

The module exports a single function:

```
import { WithProcess } from 'windows-process';
```

Its signature:

```
WithProcess(name: string, callback: function) -> nothing
```

`name` is the name of the running process you wish to inspect. For each process found with that name, `callback` is invoked. Its signature:

```
callback(process: object) -> shouldHalt: boolean
```

The `process` object provides access to the rest of the API. See below. If the return value is `true`, `WithProcess` halts after this first process matching the input name. Otherwise, it continues enumerating processes, and will invoke the callback for each one that matches. This two-step API encapsulates the process resource HANDLE, providing a guaranteed, scoped lifetime that won't leak.

## `process` API

```
getModuleBase(name: string) -> baseOffset: number
```

Returns the base memory offset of a module in process memory. This is useful if the memory address you wish to read from is a known offset into a module's memory space.

```
readMemory(address: number, countInBytes: number) -> buffer
```

Does what it says on the tin. Returns a node Buffer object.

## Example

```
import { WithProcess } from 'windows-process';
WithProcess("node.exe", process => {
    const ntDllBase = process.getModuleBase("ntdll.dll");
    const firstFourBytes = process.readMemory(ntDllBase, 4);
    console.log(ntDllBase.toString(16)); // → 7fffb0d80000
    console.log(firstFourBytes); // → <Buffer 4d 5a 90 00>
});
```

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
