

import TinyQueue from 'tinyqueue';

/**
 * Find "groups" in an ordered timeline, so that parallel 
 * events get different (low) groups (integers starting from zero). 
 * Events are defined by start- and end-date. The container, for
 * whose elements findNextFreeGroup may be called subsequentially,
 * must be ordered by start-date.
 */
export default class TimelineGroupFind {

  constructor(){
    this._lastEndDates = [];
    this._freeGroups = new TinyQueue();
  }

  /**
   * @return {int} lowest free group, starting from 0.
   * @param {Date} startDate start date of the next time element 
   * @param {Date} endDate end date of the next time element
   */
  findNextFreeGroup(startDate, endDate){
    for (let i = this._lastEndDates.length - 1; i >= 0; i--) {
      if (startDate > this._lastEndDates[i].endTime) {
        this._freeGroups.push(this._lastEndDates[i].group);
        this._lastEndDates.splice(i, 1);
      }
    }
    // if we have free groups (from previous runs) use the lowest free group, 
    // else add a new one
    const group = (this._freeGroups.length > 0) ? this._freeGroups.pop() : 
      this._lastEndDates.length;
    this._lastEndDates.push(new _LastEndDateGroup(group, endDate));
    return group;
  }
}


class _LastEndDateGroup {
  constructor(group, endTime){
    this.group = group;
    this.endTime = endTime;
  }
}
