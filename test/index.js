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
  });
});
