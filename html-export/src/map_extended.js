

export default class MapExtended extends Map {
  
  /**
   * Like get() but insert and return a default, if the key
   * does not exist
   * @return {*} 
   * @param {*} key 
   * @param {Function} defaultFactory A parameterless function whose return value
   * is used as default.
   */
  getDefault(key, defaultFactory) {
    if(defaultFactory === undefined){
      throw Error('defaultValue must not be undefined');
    }
    let val = this.get(key);
    if(val === undefined){
      val = defaultFactory();
      this.set(key, val);
    }
    return val;
  }

}
