var process = require('../');
var assert = require('assert');


describe('windows process', function() {
  it('should export a function that returns an array of running process names', function() {
    var running = process.GetRunningProcesses();
    assert.equal(Array.isArray(running), true);
    
    for (var i = 0; i < running.length; ++i) {
        console.log(running[i]);
    }
  });
});
