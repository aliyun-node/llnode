# Draft of the JavaScript API

This is currently a work in progress, expect the API to change significantly.
Specifically, the APIs returning strings as results of inspection should return
structured data instead for ease of use.

```js
class LLNode {
  /**
   * @param {string} dump path to the coredump
   * @param {string} executable path to the node executable
   * @returns {LLNode} an LLNode instance
   */
  constructor(dump, executable) {}

  /**
   * @desc LLDB load core dump
   */
  loadCore() {}

  /**
   * @desc SBProcess information
   *
   * @typedef {object} Process
   * @property {number} pid the pid of process
   * @property {number} thread_count the count of threads in the process
   * @property {string} state the state of process
   * @property {string} executable executable path
   *
   * @return {Process} process data
   */
  getProcessInfo() {}

  /**
   * @param {array|numver} thread_index ex: 1 or [0, 1, 2]
   * @param {<optional>number} current_frame
   * @param {<optional>number} limit limit of frames you want to get (start from current_frame)
   *
   * @typedef {object} JSObject
   * @property {number} type see src/llnode-common.h InspectType
   * @property {string} name
   * @property {string} address address of context
   * @property {string} map_address
   * @property other unique property will depend on js object type, see src/llnode-common.h for more information.
   *
   * @typedef {object} NativeFrame
   * @property {number} type 1
   * @property {string} name Native
   * @property {string} function function name
   * @property {string} module path of library
   * @property {string} compile_unit
   *
   * @typedef {object} JSFrame
   * @property {number} type 2
   * @property {string} name JavaScript
   * @property {string} function function name
   * @property {JSObject} context function call this
   * @property {[JSObject]} arguments function call arguments
   * @property {string} line function debug line
   * @property {string} func_addr function address
   *
   * @typedef {object} UnknownJSFrame
   * @property {number} 2
   * @property {string} name Unknown
   * @property {string} function function name
   * @property {string} func_addr ''
   *
   * @typedef {object} ThreadInfo
   * @property {boolean} frame_end
   * @property {<optional>number} frame_left if frame_end is false, will have
   * @property {NativeFrame|JSFrame|UnknownJSFrame} frame_list
   *
   * @returns {[ThreadInfo]} return threadinfo list
   */
  getThreadByIds() {}

  /**
   * @param {<optional>number} current current js object index
   * @param {<optional>number} limit limit of js objects you want to get (start from current)
   *
   * @typedef {object} TypedJSObject
   * @property {number} index
   * @property {string} name typed js object name
   * @property {number} count typed js object count
   * @property {number} size typed js object total size
   *
   * @typedef {object} TypedList
   * @property {boolean} object_end
   * @property {<optional>number} object_left if object_end is false, will have
   * @property {[TypedJSObject]} object_list
   *
   * @return {TypedList} return typed object list
   */
  getJsObjects() {}

  /**
   * @param {<optional>number} current current js instance index
   * @param {<optional>number} limit limit of js instances you want to get (start from current)
   *
   * @typedef {object} JSInstance
   * @property {string} address instance address
   * @property {string} desc short description of instance
   *
   * @typedef {object} InstanceList
   * @property {boolean} instance_end
   * @property {<optional>number} instance_end if instance_end is false, will have
   * @property {[JSInstance]} instance_list
   *
   * @return {InstanceList} return js instance list
   */
  getJsInstances() {}

  /**
   * @param {string} address
   *
   * @typedef {object} JSObject
   * @property {number} type see src/llnode-common.h InspectType
   * @property {string} name
   * @property {string} address address of context
   * @property {string} map_address
   * @property other unique property will depend on js object type, see src/llnode-common.h for more information.
   *
   * @return {JSObject} return inspected js object
   */
  inspectJsObjectAtAddress() {}
}
```
