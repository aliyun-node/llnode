'use strict';
// const LLNode = require('@alicloud/llnode-api');
const LLNode = require('..');
// console.log(LLNode)
// const llnode = new LLNode('/Users/yijun/Downloads/alinode-v3.11.2-linux-x64/bin/skylark.core',
//   '/Users/yijun/Downloads/alinode-v3.11.2-linux-x64/bin/node.node');
// s
// const llnode = new LLNode('/Users/yijun/Documents/core/core.35366',
//   '/Users/yijun/Documents/core/node');
// const llnode = new LLNode('/Users/yijun/Downloads/alinode-v3.11.2-linux-x64/bin/7930.core', 'node');
// const llnode = new LLNode('/Users/yijun/Downloads/alinode-v3.11.2-linux-x64/bin/30295.core', '/Users/yijun/Downloads/alinode-v3.11.2-linux-x64/bin/linux.3.11.2.node');
// const llnode = new LLNode('/home/admin/llnode-apiserver/oss/u-cfbd513b-7f4f-4cb3-b280-100683ef5803-u-skylark.core',
//   '/home/admin/llnode-apiserver/oss/u-b5138ea2-5501-4599-81a3-957cd1a13f30-u-node.node');
// const llnode = new LLNode('/Users/yijun/git/llnode-apiserver/oss/u-cfbd513b-7f4f-4cb3-b280-100683ef5803-u-skylark.core',
//   '/Users/yijun/git/llnode-apiserver/oss/u-b5138ea2-5501-4599-81a3-957cd1a13f30-u-node.node');
const llnode = new LLNode('/Users/yijun/Downloads/u-39a50b14-9319-4b0f-8863-679b85764570-u-skylark.core',
  '/Users/yijun/Downloads/u-fbddb08d-9198-4169-9f45-4793a1d226ac-u-linux.3.11.2.node', {
    heap_scan_monitor: function (now, total) {
      let percentage = parseInt(now / total * 100);
      console.log(12333, percentage);
    }
  });

console.log('pid:', process.pid);

llnode.loadCore();
// setInterval(() => {
//   console.log(llnode.getJsObjects());
// }, 1000);
// console.log(llnode.getProcessInfo());
// llnode.getThreadByIds(0, 0, 1)[0]
// console.log(llnode.getThreadByIds([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]));
// console.log(llnode.getJsObjects());
// console.log(llnode.getJsInstances(0, 0, 50));
// console.log(llnode.inspectJsObjectAtAddress('0x00001eec2e4894a9').properties);
// console.log(llnode.inspectJsObjectAtAddress('0x000037916f282201'));
// console.log(llnode.inspectJsObjectAtAddress('0x000015aea6be6eb1'));
// console.log(llnode.inspectJsObjectAtAddress('0x000011ad06a0a829'));
// llnode.getJsObjects();
// console.log(llnode.inspectJsObjectAtAddress('0x000011ad64497ff1', { current: 2, limit: 2 }));
// console.log(llnode.inspectJsObjectAtAddress('0x0000003ed7e02361', { current: 50, limit: 50 }));
// console.log(console.log(llnode.inspectJsObjectAtAddress('0x000011ad06a05469')))
// console.log(console.log(llnode.inspectJsObjectAtAddress('0x000011ad06a08f69')))

// console.log(llnode.getThreadByIds([0, 1], 0, 1));
// console.log(llnode.getJsObjects(0, 2));
// console.log(llnode.getThreadByIds(0, 0, 1));
// console.log(llnode.getThreadByIds(1, 0, 1));
// console.log(llnode.getThreadByIds(2, 0, 1));

const dumpPath = require('path').join(__dirname, './test.heapsnapshot');
const result = llnode.heapdump(dumpPath);
console.log(result);