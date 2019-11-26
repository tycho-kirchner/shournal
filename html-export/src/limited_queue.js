

import TinyQueue from 'tinyqueue';

/**
 * Allow for a max. length of the queue.
 * Add further convenience functions
 */
export default class LimitedQueue extends TinyQueue {

  setMaxLength(l){
    this._maxLength = l;
  }

  /**
   * @override
   */
  push(item) {
    super.push(item);
    if(this._maxLength !== undefined && this.length > this._maxLength){
      this.pop();
    }
  }

  popAll(){
    const items = [];
    while (this.length > 0) { 
      items.push(this.pop());
    }
    return items;
  }
}
