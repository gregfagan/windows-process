'use strict';

var WithProcess = require('../').WithProcess;
var assert = require('assert');

describe('window-process', () => {
  describe('WithProcess', () => {
    it('should expect a string and a callback as arguments', () => {
      assert.throws(() => { WithProcess(); }, /2 arguments/);
      assert.throws(() => { WithProcess('node.exe'); }, /2 arguments/);
      assert.throws(() => { WithProcess(0, () => {}); }, /process name/);
      assert.throws(() => { WithProcess('node.exe', 0); }, /callback function/);
    });
    it('should find a process by name and invoke callback', done => {
      WithProcess('node.exe', () => {
        done();
        return true;
      });
    });
    it('should provide the callback with a process object', done => {
      WithProcess('node.exe', process => {
        assert(process.getModuleBase);
        assert(process.readMemory);
        
        done();
        return true;
      });
    });
    it('should not allow references to persist beyond scope of callback', () => {
      let leakedProcess;
      WithProcess('node.exe', process => {
        leakedProcess = process;
        return true;
      });
      
      // Cannot access process functions outside of callback scope
      assert(leakedProcess);
      assert.equal(leakedProcess.getModuleBase, undefined);
      assert.equal(leakedProcess.readMemory, undefined);
    });
    describe('process.getModuleBase', () => {
      it('should find a well-known module offset', done => {
        WithProcess('node.exe', process => {
          assert(process.getModuleBase('ntdll.dll') > 0);
          
          done();
          return true;
        });
      });
    });
    describe('process.readMemory', () => {
      it('should return a buffer', done => {
        WithProcess('node.exe', process => {
          const base = process.getModuleBase('ntdll.dll');
          const buffer = process.readMemory(base, 4);
          
          assert(buffer instanceof Buffer);
          
          done();
          return true;
        });
      });
    });
  });
});


