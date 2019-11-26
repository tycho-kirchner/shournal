
export class ErrorNotImplemented extends Error { 
  constructor() {
    super('Required method not implemented');
  }
}

export function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export function getTime() {
  return new Date().getTime();
}

export function date_max(d1, d2){
  return d1 > d2 ? d1 : d2;
}

export function date_min(d1, d2){
  return d1 < d2 ? d1 : d2;
}

export function windowWidth() {
  return window.innerWidth ||
    document.documentElement.clientWidth ||
    document.body.clientWidth;
}


export function windowHeight() {
  return window.innerHeight ||
    document.documentElement.clientHeight ||
    document.body.clientHeight;
}


export function assert(condition, message) {
  if (!condition){
    throw Error('Assert failed: ' + (message || ''));
  }
}

export const DATE_MIN = new Date(-8640000000000000);

/**
 * non-blocking .foreach array loop.
 * @param {*} array 
 * @param {*} func 
 */
export async function timedForEach(array, func) {
  const maxTimePerChunk = 200; // max 200ms until next sleep
  function getTime() {
    return new Date().getTime();
  }
  
  let lastStart = getTime();
  for (let i=0; i < array.length; i++) {
    func(array[i], i, array); 
    const now = getTime();
    if(now - lastStart > maxTimePerChunk){
      // enough computation time used
      await sleep(5);
      lastStart = now;
    }
  }
}


/**
 * Binary search.
 * @param {[]} ar sorted array, may contain duplicate elements.
 * If there are more than one equal elements in the array,
 * the returned value can be the index of any one of the equal elements.
 * @param {*} el element to search for
 * @param {function}  compareFn  A comparator function. The function takes two arguments: (a, b) and returns:
 *        a negative number  if a is less than b;
 *        0 if a is equal to b;
 *        a positive number of a is greater than b.
 * @param {boolean} clipIdx see @return: 
 * @return {int} if clipIdx is false: index of of the element in a sorted array or (-n-1) where n
 * is the insertion point for the new element. 
 * If clipIdx is true: return an index within the array element bounds, independent of
 * wheter the element exists or not (the best matching existing index is returned).
 */
export function binarySearch(ar, el, compareFn, clipIdx=false) {
  const clipIdxIfOn = (idx) => {
    if(! clipIdx){
      return idx;
    }
    if (idx < 0) {
      idx = -(idx + 1);
    }
    if (idx >= ar.length) {
      return ar.length - 1;
    }
    return idx;
  };
  
  let m = 0;
  let n = ar.length - 1;
  while (m <= n) {
    const k = (n + m) >> 1;
    const cmp = compareFn(el, ar[k]);
    if (cmp > 0) {
      m = k + 1;
    } else if(cmp < 0) {
      n = k - 1;
    } else {
      return clipIdxIfOn(k);
    }
  }
  return clipIdxIfOn(-m - 1);
}

/**
 * Get the directry of a unix path, e.g. the path /home/user/foo
 * would return /home/user.
 * @return {String}
 * @param {String} path 
 */
export function getDirFromAbsPath(path){
  return path.substring(0,path.lastIndexOf("/"));
}


