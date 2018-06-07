'use strict';

const llnodex = require('bindings')('llnodex');
const fromCoredump = llnodex.fromCoredump;
const LLNodeHeapType = llnodex.LLNodeHeapType;
const nextInstance = llnodex.nextInstance;

function *next() {
  let instance;
  while (instance = nextInstance(this)) {
    yield instance;
  }
}

Object.defineProperty(LLNodeHeapType.prototype, 'instances', {
  enumerable: false,
  configurable: false,
  get: function() {
    return {
      [Symbol.iterator]: next.bind(this)
    }
  }
});

module.exports = {
  fromCoredump
}
