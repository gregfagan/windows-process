'use strict';

var ReadFromProcess = require('../').ReadFromProcess;
var assert = require('assert');

describe('window-process', () => {
  describe('ReadFromProcess', () => {
    it('should find a process by name and invoke callback', done => {
      ReadFromProcess("node.exe", () => {
        done();
        return true;
      });
    });
    it('should provide the callback with a process object', () => {
      ReadFromProcess("node.exe", process => {
        assert(process.GetModuleBase);
        return true;
      });
    });
    it('should empty the process object when done', () => {
      let leakedProcess;
      ReadFromProcess("node.exe", process => {
        leakedProcess = process;
        return true;
      });
      assert.equal(leakedProcess.GetModuleBase, undefined);
    });
    describe('process.GetModuleBase', () => {
      it('should find a well-known module offset', () => {
        ReadFromProcess("node.exe", process => {
          let base = process.GetModuleBase("ntdll.dll");
          assert(base > 0);
          return true;
        })
      })
    })
  });
});
