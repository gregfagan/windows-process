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
      it('should expect a string as an argument', () => {
        WithProcess('node.exe', process => {
          assert.throws(() => { process.getModuleBase(); }, /module name/);
          assert.throws(() => { process.getModuleBase(0); }, /module name/);
        })
      })
      it('should return the offset of a well-known module', done => {
        WithProcess('node.exe', process => {
          const base = process.getModuleBase('ntdll.dll');
          assert(typeof base === 'number');
          assert(base > 0);
          
          done();
          return true;
        });
      });
    });
    describe('process.readMemory', () => {
      it('should expect two numbers as arguments', () => {
        WithProcess('node.exe', process => {
          assert.throws(() => { process.readMemory(); }, /2 arguments/);
          assert.throws(() => { process.readMemory(0); }, /2 arguments/);
          assert.throws(() => { process.readMemory("test", 0); }, /First.*number/);
          assert.throws(() => { process.readMemory(0, "test"); }, /Second.*number/);
        });
      });
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


