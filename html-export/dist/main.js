/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, { enumerable: true, get: getter });
/******/ 		}
/******/ 	};
/******/
/******/ 	// define __esModule on exports
/******/ 	__webpack_require__.r = function(exports) {
/******/ 		if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 			Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 		}
/******/ 		Object.defineProperty(exports, '__esModule', { value: true });
/******/ 	};
/******/
/******/ 	// create a fake namespace object
/******/ 	// mode & 1: value is a module id, require it
/******/ 	// mode & 2: merge all properties of value into the ns
/******/ 	// mode & 4: return value when already ns object
/******/ 	// mode & 8|1: behave like require
/******/ 	__webpack_require__.t = function(value, mode) {
/******/ 		if(mode & 1) value = __webpack_require__(value);
/******/ 		if(mode & 8) return value;
/******/ 		if((mode & 4) && typeof value === 'object' && value && value.__esModule) return value;
/******/ 		var ns = Object.create(null);
/******/ 		__webpack_require__.r(ns);
/******/ 		Object.defineProperty(ns, 'default', { enumerable: true, value: value });
/******/ 		if(mode & 2 && typeof value != 'string') for(var key in value) __webpack_require__.d(ns, key, function(key) { return value[key]; }.bind(null, key));
/******/ 		return ns;
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 2);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";

Object.defineProperty(exports, "__esModule", { value: true });
var Mutex_1 = __webpack_require__(1);
exports.Mutex = Mutex_1.default;


/***/ }),
/* 1 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";

Object.defineProperty(exports, "__esModule", { value: true });
var Mutex = /** @class */ (function () {
    function Mutex() {
        this._queue = [];
        this._pending = false;
    }
    Mutex.prototype.isLocked = function () {
        return this._pending;
    };
    Mutex.prototype.acquire = function () {
        var _this = this;
        var ticket = new Promise(function (resolve) { return _this._queue.push(resolve); });
        if (!this._pending) {
            this._dispatchNext();
        }
        return ticket;
    };
    Mutex.prototype.runExclusive = function (callback) {
        return this
            .acquire()
            .then(function (release) {
            var result;
            try {
                result = callback();
            }
            catch (e) {
                release();
                throw (e);
            }
            return Promise
                .resolve(result)
                .then(function (x) { return (release(), x); }, function (e) {
                release();
                throw e;
            });
        });
    };
    Mutex.prototype._dispatchNext = function () {
        if (this._queue.length > 0) {
            this._pending = true;
            this._queue.shift()(this._dispatchNext.bind(this));
        }
        else {
            this._pending = false;
        }
    };
    return Mutex;
}());
exports.default = Mutex;


/***/ }),
/* 2 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);

// CONCATENATED MODULE: ./src/generic_text_dialog.js


class GenericTextDialog {
  constructor() {
  }

  show(title, content){
    $("#genericModalTitle").html(title);
    $("#genericModalBody").html(content);
    $("#genericModal").modal('toggle');
  }
}

// CONCATENATED MODULE: ./src/globals.js




let humanDateFormat;
let humanDateFormatOnlyDate;
let humanDateFormatOnlyTime;

let d3TimeParseIsoWithMil;

let textDialog;

let commandList;
let sessionTimeline;

function init(){
  humanDateFormat = d3.timeFormat("%Y-%m-%d %H:%M");
  humanDateFormatOnlyDate = d3.timeFormat("%Y-%m-%d");
  humanDateFormatOnlyTime = d3.timeFormat("%H:%M");

  d3TimeParseIsoWithMil = d3.timeParse("%Y-%m-%dT%H:%M:%S.%L");

  textDialog = new GenericTextDialog();
}

// CONCATENATED MODULE: ./src/command_manipulation.js



/**
 * Parse the command-date into d3's date and assign session colors
 * @param {[Command]} commands
 */
function prepareCommands(commands){
  commands.forEach(function(cmd) {
    cmd.startTime = d3TimeParseIsoWithMil(cmd.startTime);
    cmd.endTime = d3TimeParseIsoWithMil(cmd.endTime);
  });
  _fillCommandSessionColors(commands);
}

/**
 * Can be passed to array.sort or similar functions.
 * @param {*} cmd1 
 * @param {*} cmd2 
 * @return {int}
 */
function compareStartDates(cmd1, cmd2) {
  return cmd1.startTime - cmd2.startTime;
}

/**
 * Can be passed to array.sort or similar functions.
 * @param {*} cmd1 
 * @param {*} cmd2 
 * @return {int}
 */
function compareEndDates(cmd1, cmd2) {
  return cmd1.endTime - cmd2.endTime;
}


/**
 * Assign session-colors to the given commands.
 * Each session gets a specific color, after n sessions occurred, colors
 * start from beginning again.
 *  @param {[Command]} commands
*/
function _fillCommandSessionColors(commands){
  const DISTINCT_COLORS = [
    '#e6194b', '#3cb44b', '#ffe119', '#4363d8', '#f58231', '#911eb4', '#46f0f0',
    '#f032e6', '#bcf60c', '#fabebe', '#008080', '#e6beff', '#9a6324', '#fffac8',
    '#800000', '#aaffc3', '#808000', '#ffd8b1', '#000075', '#808080',
  ];
  let lastColorIdx = 0;
  const sessionColorMap = new Map();
  commands.forEach(function(cmd) {
    if(cmd.sessionUuid === null){
      cmd.sessionColor = '#000000';
    } else {
      let color = sessionColorMap.get(cmd.sessionUuid);
      if(color === undefined){
        color = DISTINCT_COLORS[lastColorIdx];
        sessionColorMap.set(cmd.sessionUuid, color);
        lastColorIdx++;
        if(lastColorIdx >= DISTINCT_COLORS.length){
          lastColorIdx = 0;
        }
      }
      cmd.sessionColor = color;
    }
  });
}

// CONCATENATED MODULE: ./src/html_util.js


function insertAfter(newNode, referenceNode) {
  referenceNode.parentNode.insertBefore(newNode, referenceNode.nextSibling);
}


/**
 * Check if element is visible inside container - also partially at your wish.
 * @return {boolean}
 * @param {Element} element 
 * @param {Element} container 
 * @param {boolean} partial if true, return true, if not completely but partially
 * visible
 */
function isScrolledIntoView(element, container, partial) {
   // Get container properties
   const cTop = container.scrollTop;
   const cBottom = cTop + container.clientHeight;

   // Get element properties
   const eTop = element.offsetTop;
   const eBottom = eTop + element.clientHeight;

   // Check if in view    
   const isTotal = (eTop >= cTop && eBottom <= cBottom);
   const isPartial = partial && (
     (eTop < cTop && eBottom > cTop) ||
     (eBottom > cBottom && eTop < cBottom)
   );

   return (isTotal || isPartial);
}

// CONCATENATED MODULE: ./src/util.js

class ErrorNotImplemented extends Error { 
  constructor() {
    super('Required method not implemented');
  }
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function getTime() {
  return new Date().getTime();
}

function date_max(d1, d2){
  return d1 > d2 ? d1 : d2;
}

function date_min(d1, d2){
  return d1 < d2 ? d1 : d2;
}

function windowWidth() {
  return window.innerWidth ||
    document.documentElement.clientWidth ||
    document.body.clientWidth;
}


function windowHeight() {
  return window.innerHeight ||
    document.documentElement.clientHeight ||
    document.body.clientHeight;
}


function assert(condition, message) {
  if (!condition){
    throw Error('Assert failed: ' + (message || ''));
  }
}

const DATE_MIN = new Date(-8640000000000000);

/**
 * non-blocking .foreach array loop.
 * @param {*} array 
 * @param {*} func 
 */
async function timedForEach(array, func) {
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
function binarySearch(ar, el, compareFn, clipIdx=false) {
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
function getDirFromAbsPath(path){
  return path.substring(0,path.lastIndexOf("/"));
}



// CONCATENATED MODULE: ./src/conversions.js

/**
 * @return {String} human readble byte-size-string
 * @param {int} bytes 
 * @param {boolean} si if true: use 1000 as base (kB), else 1024 (KiB)
 */
function bytesToHuman(bytes, si = false) {
  const thresh = si ? 1000 : 1024;
  if (Math.abs(bytes) < thresh) {
    return bytes + ' B';
  }
  const units = si ?
   ['kB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'] :
   ['KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB', 'ZiB', 'YiB'];
  let u = -1;
  do {
    bytes /= thresh;
    ++u;
  } while (Math.abs(bytes) >= thresh && u < units.length - 1);
  return bytes.toFixed(1) + ' ' + units[u];
}

// CONCATENATED MODULE: ./src/command_list.js






class command_list_CommandList {
  constructor(commands) {

    this._CMDLISTPADDING = 18;
    this._CMDLISTBG = '#777';

    const cmdListHeight = (() => {
      const boundClient = sessionTimeline.getSvg().node().getBoundingClientRect();
      let h = windowHeight() - (boundClient.y + boundClient.height) - 30; // why minus 30?
      if (h < 200) {
        // screen too small (or too many command groups): allow for scrolling
        h = 300;
      }
      return h;
    })();

    const cmdListScroll = d3.select('body').append('div')
      .attr('id', 'cmdListScroll')
      .style('height', cmdListHeight + 'px');


    cmdListScroll.selectAll('.collapsibleCmd')
      .data(commands)
      .enter()
      .append('button')
      .attr('class', 'collapsibleCmd')
      .attr('id', (cmd) => { return 'cmdListEntry' + cmd.id; })
      .html((cmd) => {
        // only display year,month,day of endTime if different from start
        const actualEndFormat = (cmd.startTime.getFullYear() == cmd.endTime.getFullYear() &&
          cmd.startTime.getMonth() == cmd.endTime.getMonth() &&
          cmd.startTime.getDay() == cmd.endTime.getDay()
        ) ? humanDateFormatOnlyTime : humanDateFormat;
        return humanDateFormat(cmd.startTime) + ' - ' +
          actualEndFormat(cmd.endTime) + ': ' +
          cmd.command;
      })
      .style('padding', this._CMDLISTPADDING + 'px')
      .style('background', (cmd) => { return this._computeCmdBackground(cmd); })
      .on("click", (cmd, idx) => {
        if (document.readyState !== "complete"){
          // silently ignore clicks, until everything loaded...
          return;
        }

        this._handleClickOnCmd(cmd, idx);
      });
  }

  /**
  * @param {*} cmd command-object to scroll to
  */
  scrollToCmd(cmd) {
    const cmdElement = this._selectCmdEntry(cmd);
    const scroll = document.getElementById('cmdListScroll');
    scroll.scrollTop = cmdElement.node().offsetTop;

    cmdElement
      .transition()
      .duration(1300) // miliseconds
      .style("background", "red")
      .on("end", () => { 
        cmdElement.style('background', (cmd) => { return this._computeCmdBackground(cmd); });
      });
  }

  _selectCmdEntry(cmd){
    return d3.select(`#cmdListEntry${cmd.id}`);
  }

  _computeCmdBackground(cmd){
    return `linear-gradient(to right,
      ${cmd.sessionColor} 0px, ${cmd.sessionColor} ${this._CMDLISTPADDING - 1}px,
      ${this._CMDLISTBG} ${this._CMDLISTPADDING - 1}px, ${this._CMDLISTBG} 100%)`;
  }

  _handleClickOnCmd(cmd, idx){
    let contentDiv = d3.select(`#cmdcontent${cmd.id}`);
    if (! contentDiv.empty()) {
      contentDiv.remove();
      return;
    }

    contentDiv = d3.select('body').append('div')
      .attr('id', `cmdcontent${cmd.id}`)
      .attr('class', 'collapsibleCmdContent')
      .html(`Working directory: ${cmd.workingDir}<br>` +
        `Command exit status: ${cmd.returnValue}<br>` +
        `Session uuid: ${cmd.sessionUuid}<br>` +
        `Command id: ${cmd.id}<br>` +
        `Hostname: ${cmd.hostname}<br>`);

    const alternatingColor = '#D9D9D9';
    
    if (cmd.fileWriteEvents_length > 0) {
      contentDiv.append('span')
        .html(cmd.fileWriteEvents_length + ' written files')
        .style('color', 'red')
        .style('display', 'block');
      
      contentDiv.selectAll('.nonexistentClass')
        .data(cmd.fileWriteEvents)
        .enter()
        .append('span')
        .style('display', 'block')
        .style('background-color', (e, idx) => {
          return (idx % 2 === 0) ? 'transparent' : alternatingColor;
        })
        .text((e) => {
          return `${e.path} (${bytesToHuman(e.size)}), Hash: ${e.hash}`;
        });

      if (cmd.fileWriteEvents.length !== cmd.fileWriteEvents_length) {
        contentDiv.append('span').html(
          `... and ` +
          `${cmd.fileWriteEvents_length - cmd.fileWriteEvents.length}` +
          ` more (see shournal's query help to increase limits)<br>`);
      }
    }

    if (cmd.fileReadEvents_length > 0) {
      if(cmd.fileWriteEvents_length > 0){
         contentDiv.append('span').html('<br>');
      }
      contentDiv.append('span')
      .html(cmd.fileReadEvents_length + ' read files')
      .style('color', 'red')
      .style('display', 'block');
    }
 
    contentDiv.selectAll('.nonexistentClass')
      .data(cmd.fileReadEvents)
      .enter()
      .append('span')
      .style('background-color', (e, idx) => {
        return (idx % 2 === 0) ? 'transparent' : alternatingColor;
      })
      .style('color', (readFile) => { return (readFile.isStoredToDisk) ? 'blue' : 'black'; })
      .style('cursor', (readFile) => { return (readFile.isStoredToDisk) ? 'pointer' : 'default'; })
      .style('display', 'block') // only one read file per line
      .text((e) => { return `${e.path} (${bytesToHuman(e.size)}), Hash: ${e.hash}`; })
      .on("click", (readFile) => {
        if (readFile.isStoredToDisk) {
          const mtimeHuman = humanDateFormat(d3.isoParse(readFile.mtime));
          const title = `Read file ${readFile.path}<br>` +
                        `mtime: ${mtimeHuman}<br>` +
                        `size: ${bytesToHuman(readFile.size)}<br>` + 
                        `hash: ${readFile.hash}<br>`;

          const readFileContent = atob(readFileContentMap.get(readFile.id));
          textDialog.show(title, readFileContent);
        }
      });
    
    if (cmd.fileReadEvents.length !== cmd.fileReadEvents_length) {
      contentDiv.append('span').html(
        `... and ` +
        `${cmd.fileReadEvents_length - cmd.fileReadEvents.length}` +
        ` more (see shournal's query help to increase limits)<br>`
      );
    }

      
    const cmdElement = this._selectCmdEntry(cmd);
    insertAfter(contentDiv.node(), cmdElement.node());

    const cmdListScroll = document.getElementById('cmdListScroll');
    if(! isScrolledIntoView(contentDiv.node(), cmdListScroll, true)){
      // scroll down one element, so at least the beginning of content is visible:
      cmdListScroll.scrollTop += cmdElement.node().clientHeight;  
    } 
  }
}

// CONCATENATED MODULE: ./src/map_extended.js


class MapExtended extends Map {
  
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

// CONCATENATED MODULE: ./node_modules/tinyqueue/index.js

class TinyQueue {
    constructor(data = [], compare = defaultCompare) {
        this.data = data;
        this.length = this.data.length;
        this.compare = compare;

        if (this.length > 0) {
            for (let i = (this.length >> 1) - 1; i >= 0; i--) this._down(i);
        }
    }

    push(item) {
        this.data.push(item);
        this.length++;
        this._up(this.length - 1);
    }

    pop() {
        if (this.length === 0) return undefined;

        const top = this.data[0];
        const bottom = this.data.pop();
        this.length--;

        if (this.length > 0) {
            this.data[0] = bottom;
            this._down(0);
        }

        return top;
    }

    peek() {
        return this.data[0];
    }

    _up(pos) {
        const {data, compare} = this;
        const item = data[pos];

        while (pos > 0) {
            const parent = (pos - 1) >> 1;
            const current = data[parent];
            if (compare(item, current) >= 0) break;
            data[pos] = current;
            pos = parent;
        }

        data[pos] = item;
    }

    _down(pos) {
        const {data, compare} = this;
        const halfLength = this.length >> 1;
        const item = data[pos];

        while (pos < halfLength) {
            let left = (pos << 1) + 1;
            let best = data[left];
            const right = left + 1;

            if (right < this.length && compare(data[right], best) < 0) {
                left = right;
                best = data[right];
            }
            if (compare(best, item) >= 0) break;

            data[pos] = best;
            pos = left;
        }

        data[pos] = item;
    }
}

function defaultCompare(a, b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

// CONCATENATED MODULE: ./src/timeline_group_find.js




/**
 * Find "groups" in an ordered timeline, so that parallel 
 * events get different (low) groups (integers starting from zero). 
 * Events are defined by start- and end-date. The container, for
 * whose elements findNextFreeGroup may be called subsequentially,
 * must be ordered by start-date.
 */
class timeline_group_find_TimelineGroupFind {

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

// EXTERNAL MODULE: ./node_modules/async-mutex/lib/index.js
var lib = __webpack_require__(0);

// CONCATENATED MODULE: ./src/annotation_line_render.js






/**
 * Render Groups of annotations on a per-line-basis. Clip annotation texts 
 * and omit annotations as needed to fit into available space
 */
class annotation_line_render_AnnotationLineRender {
  constructor(plot) {
    this._annotationGroups = [];
    this._plot = plot;
    // get the width in pixel of a character
    this._annotationCharWidth = d3.select("#annotation_text_char").node()
      .getBoundingClientRect().width;
    // do not render an annotation which does not fit into the space.
    this._annotationMinWidth = this._annotationCharWidth * 2;
    // clip annotation-texts after that many characters
    this._annotationMaxNumChars = 15;
    this._updateMutex = new lib["Mutex"]();
    this._lastUpdateDummy = null;
  }

  /**
   * 
   * @param {Array<Annotation>} group: ordered set of annotations which will be rendered
   * within the same line. Base class is the same as d3 annotation, however, the following
   * *additional* fields must be set: startX, endX, fulltext. The annotation position
   * (x,y) has to be set already, based on the x-values it is decided, how much of
   * an annotation is drawn.
   */
  addAnnotationGroup(group) {
    this._annotationGroups.push(group);
  }


  async update(xScale) {
    this._lastUpdateDummy = {};
    const currentUpdateDummy = this._lastUpdateDummy;

    const release = await this._updateMutex.acquire();
    try {
      // remove and add again seems to be faster than updating
      this._plot.selectAll('.annotation').remove();
      this._plot.selectAll('.annotationVertLine').remove();
      this._plot.selectAll('.annotationHorizLine').remove();

      const annotations = await this._preRenderAnnotations(xScale, currentUpdateDummy);
      if (annotations !== null) {
        this._appendAnnotations(annotations);
      }
    } finally {
      release();
    }
  } 

  setOnNoteClick(func){
    this._onNoteClick = func;
  }

  // ***************** PRIVATE ********************

  _compareStartX(prev, current) {
    return prev.startX - current.startX;
  }  

  _compareEndX(prev, current) {
    return prev.endX - current.endX;
  }  

  async _preRenderAnnotations(xScale, currentUpdateDummy ) {  
    
    const annotations = [];
    // uniform interface for binary search, where the entrance indeces are found
    const dummyAnnotation = {
      startX: xScale.domain()[0],
      endX: xScale.domain()[1],
    };

    const plotWidth = this._plot.node().getBBox().width;
    for(const annotationLine of this._annotationGroups) {
      if (annotationLine.length == 0) {
        continue;
      }
      // Do not render annotations outside the current view
      // -> find start and stop indeces in the group:
      // Note: one cannot simply choose 0 and length -1 after zooming
      // out, because panning also has to be respected.
      const startIdx = binarySearch(annotationLine, dummyAnnotation, 
        this._compareStartX, true);
      const endIdx = binarySearch(annotationLine, dummyAnnotation, 
        this._compareEndX, true);

      let displayAnnotation = annotationLine[startIdx];
      displayAnnotation.x = this._calcAnnotationCenter(displayAnnotation, xScale);

      for (let idx = startIdx + 1; idx <= endIdx; idx++) {
        // this.update is run async: check if it was called in between. If that's the
        // case we can abort, because or xScale is outdated.
        if (currentUpdateDummy !== this._lastUpdateDummy) {
          return null;
        }  
        
        if(idx % 30 === 0){
          // avoid freezing the DOM...
          await sleep(5);
        }        

        const annotation = annotationLine[idx];
        annotation.x = this._calcAnnotationCenter(annotation, xScale);

        const textspace = annotation.x - displayAnnotation.x -
          (this._annotationCharWidth * 2); // subtract more chars to leave space to next annotation
        const annotationTxt = this._generateAnnotationTxt(textspace, displayAnnotation.fulltext);
        if (annotationTxt == null) {
          // do not render this annotation
          continue;
        }
        // always update text, we might have zoomed before!
        displayAnnotation.note.label = annotationTxt;
        annotations.push(displayAnnotation);
        displayAnnotation = annotation;
      }

      // still need to push the final annotation, if it fits into our plot
      const textspace = plotWidth - displayAnnotation.x;
      const annotationTxt = this._generateAnnotationTxt(textspace, displayAnnotation.fulltext);
      if (annotationTxt != null) {
        displayAnnotation.note.label = annotationTxt;
        annotations.push(displayAnnotation);
      }
    }
    return annotations;
  }

  _calcAnnotationCenter(annotation, xScale) {
    return (xScale(annotation.startX) + xScale(annotation.endX)) / 2.0;
  }

    /**
   * @param {*} textspace Available width in pixel
   * @param {*} txt The full text
   * @return {*} null, if textspace was too small, else the full or clipped text
   */
  _generateAnnotationTxt(textspace, txt) {
    if (textspace < this._annotationMinWidth) {
      return null;
    }

    // Render only so many chars that fit into the space, but not more than
    // _annotationMaxNumChars;
    const maxCountOfRenderChars = Math.min(Math.ceil(textspace / this._annotationCharWidth) , 
      this._annotationMaxNumChars);
    
    if (txt.length <= maxCountOfRenderChars ) {
      return txt;
    }
    return txt.substring(0, maxCountOfRenderChars - 1) + '.';
  }


  /**
   * Append all annotations to the plot and setup mouse event handlers
   * @param {[annotation]} annotations
   */
  _appendAnnotations(annotations) {
    const enterSelection = this._plot.selectAll(".annotation")
      .data(annotations)
      .enter();

    enterSelection
      .append("text")
      .attr('class', 'annotation unselectable' )
      .attr('x', (a) => { return a.x; })
      .attr('y', (a) => { return a.ny; })
      .text((a) => { return a.note.label; })
      .attr('title', (a) => { return a.fulltext; })
      .style('cursor', 'pointer')
      .on("click", (a) => {
        if (this._onNoteClick !== undefined) {
          // d3.event.pageX, d3.event.pageY
          this._onNoteClick(a.data);
        }
      });

    // dynamically inserted elements -> rerun tooltip
    $('.annotation').tooltip({
      delay: { show: 100, hide: 0 },
    });

    const horzLineYOffset = 2;  

    const lineColor = 'steelblue';

    enterSelection
      .insert("line")
      .attr('class', 'annotationVertLine')
      .attr('x1', (a) => { return a.x; })
      .attr('y1', (a) => { return a.ny + horzLineYOffset; })
      .attr('x2', (a) => { return a.x; })
      .attr('y2', (a) => { return a.y; })
      .attr("stroke-width", 0.5)
      .attr("stroke", lineColor);
      
    enterSelection
      .insert("line")
      .attr('class', 'annotationHorizLine')
      .attr('x1', (a) => { return a.x; })
      .attr('y1', (a) => { return a.ny + horzLineYOffset; })
      .attr('x2', (a) => { return a.x+ (a.note.label.length * this._annotationCharWidth); })
      .attr('y2', (a) => { return a.ny + horzLineYOffset; })
      .attr("stroke-width", 0.5)
      .attr("stroke", lineColor);   

  }


}

// CONCATENATED MODULE: ./src/zoom_buttons.js


class ZoomButtons {
  
  /**
   * @param {d3-element} containerDiv The plot/svg is excepted to be in that div. 
   * Its 'position' should be 'relative', see https://stackoverflow.com/a/10487329
   * so we can place the buttons in an absolute manner.
   * @param {d3-element} zoomArea the element used for zooming
   * @param {d3.zoom} d3Zoom 
   */
  constructor(containerDiv, zoomArea, d3Zoom) {
    const btnGroup = containerDiv.append('div');

    const zoomInBtn = this._appendZoomButton(btnGroup, '+')
      .on("click", () => {
        d3Zoom.scaleBy(zoomArea.transition().duration(10), 1.2);
      });
    const zoomInBtnWidth = parseInt(zoomInBtn.style('width'), 10);

    const zoomOutBtn = this._appendZoomButton(btnGroup, '-')
      .on("click", () => {
        d3Zoom.scaleBy(zoomArea.transition().duration(10), 0.8);
      });
    const zoomOutBtnWidth = parseInt(zoomOutBtn.style('width'), 10);

    const zoomResetBtn = this._appendZoomButton(btnGroup, '[ ]')
      .on("click", () => {
        d3Zoom.transform(zoomArea, d3.zoomIdentity.translate(0, 0).scale(1.0));
      });
    const zoomResetBtnWidth = parseInt(zoomResetBtn.style('width'), 10);

    const zoomButtonsWidth = zoomInBtnWidth + zoomOutBtnWidth + zoomResetBtnWidth;

    btnGroup.style('position', 'absolute') // see https://stackoverflow.com/a/10487329 -> 
                                           // parent position should be relative
      .style('top', 0 + 'px')
      .style('right', ( zoomButtonsWidth) + 'px');
      
  }

  _appendZoomButton(container, text) {
    return container.append('button')
      .attr('class', 'zoomButton')
      .html(text);
  }
}

// CONCATENATED MODULE: ./src/session_timeline.js







class session_timeline_SessionTimeline {
  constructor(commands, cmdFinalEndDate) {
    this.cmdFinalEndDate = cmdFinalEndDate;

    this._margin = {
      top: 20,
      right: 20,
      bottom: 24,
      left: 24,
    };

    // get the width in pixel of a character
    this.annotationCharWidth = d3.select("#annotation_text_char").node()
      .getBoundingClientRect().width;
    this.annotationCharHeight = d3.select("#annotation_text_char").node()
      .getBoundingClientRect().height;

    // height of a session with no forks (parallel commands )
    this.sessionBaseHeight = this.annotationCharHeight / 1.5;
    this.sessionPadding = this.annotationCharHeight / 5;
    // choose less than two, so two parallel commands
    // are already wider than a lonely command.
    this.sessionMinHeight = this.sessionBaseHeight / 1.5;

    // An annotation shall only be displayed, if its minimum width in pixel
    // is at least 5 character. Warning: do not set < 1 -> text rendering issues for annotations
    this.annotationMinWidth = this.annotationCharWidth * 5;
    // distance to the belonging command rect
    this.annotationDistance = this.annotationCharHeight / 3.0;
    this.commandRects = [];

    
    // minimum width of a cmd-rect. Let it be at least 1, otherwise very short commands
    // are barely visible (get another color...)
    this.CMD_MIN_WIDTH = 4;

    this.svgWidth = windowWidth() - this._margin.left - this._margin.right - 30;

    const plotContainer = d3.select('body').append('div')
      .style('position', 'relative'); // see https://stackoverflow.com/a/10487329

    this.svg = plotContainer.append('svg');
    this._annotationRender = new annotation_line_render_AnnotationLineRender(this.svg);

    const groupedSessions = this._generateCommandsPerSession(commands);
    this.svgHeight = Math.max(100, this._prerenderSessions(groupedSessions));

    this.xScale = d3.scaleTime()
      .range([0, this.svgWidth]);

    this._yScale = d3.scaleLinear()
      .range([this.svgHeight, 0]);
    this._yScale.domain([0, this.svgHeight]);

    this.axisBottom = d3.axisBottom(this.xScale);

    this.svg.attr('width', this._margin.left + this.svgWidth + this._margin.right)
    .attr('height', this._margin.top + this.svgHeight + this._margin.bottom)
    .append('g')
    .attr('transform', 'translate(' + this._margin.left + ',' + this._margin.top + ')')
    .style('z-index', -1);

    const listenerRect = this.svg
      .append('rect')
      .attr('class', 'listener-rect')
      .attr('x', 0)
      .attr('y', -this._margin.top)
      .attr('width', this._margin.left + this.svgWidth + this._margin.right)
      .attr('height', this._margin.top + this.svgHeight + this._margin.bottom)
      .style('opacity', 0);


    this.xScale.domain([
      // the commands are sorted by starttime...
      commands[0].startTime,
      this.cmdFinalEndDate,
    ]).nice();

    // draw axes
    this.xAxisDraw = this.svg.insert('g', ':first-child')
      .attr('class', 'x axis')
      .attr('transform', 'translate(0,' + this.svgHeight + ')')
      .call(this.axisBottom
        // .ticks(d3.timeWeek, 2)
        // .tickFormat(d3.timeFormat('%b %d'))
      );

    const _drawSession = (session, idx, lineIdx) => {
      // draw rectangles
      const className = 'sessionTimeSeries' + 
        session.getSessionGroup() + idx;
      this.commandRects.push(this.svg.selectAll('.' + className)
        .data(session.getCmdsWithMeta())
        .enter()
        .append('rect')
        .attr('class', className)
        .attr('x', (cmdWithMeta) => { 
          return this._calcRectXPosition(cmdWithMeta.cmd, this.xScale); 
        })
        .attr('y', (cmdWithMeta) => { 
          // rects are drawn from top to bottom, so add the height:
          return this._yScale(cmdWithMeta.getY() + cmdWithMeta.getHeight()); 
        })
        .attr('width', (cmdWithMeta) => { 
          return this._calcRectWidth(cmdWithMeta.cmd, this.xScale); 
        })
        .attr('height', (cmdWithMeta) => { 
          return cmdWithMeta.getHeight(); 
        })
        .attr('fill', (cmdWithMeta) => { 
          // TODO: rather determine the session color in this class
          // on a per line-basis, so the same color appears as seldom
          // as possible in a given line (?).
          // But what about the colors in the cmd-list?...
          return cmdWithMeta.cmd.sessionColor; 
        } )
        .style('cursor', 'pointer')
        .attr('title', (cmdWithMeta) => { return cmdWithMeta.cmd.command; })
        .on("click", (cmdWithMeta) => { 
          commandList.scrollToCmd(cmdWithMeta.cmd); 
        })
        );
      $('.' + className).tooltip({
        delay: { show: 50, hide: 0 },
      });

    }; 

    groupedSessions.forEach((sessionLine, lineIdx) => {
      sessionLine.forEach((session, sessionIdx) => {
        _drawSession(session, sessionIdx, lineIdx);
      });
    });   


    this._preRenderAnnotations(groupedSessions);
    this._annotationRender.setOnNoteClick((cmdWithMeta) => {
      commandList.scrollToCmd(cmdWithMeta.cmd);
    });
    this._annotationRender.update(this.xScale);
        

    const minTimeMilli = 20000; // do not allow zooming beyond displaying 20 seconds
    const maxTimeMilli = 6.3072e+11; // approx 20 years

    const currentWidthMilli = cmdFinalEndDate - commands[0].startTime;

    const minScaleFactor = currentWidthMilli / maxTimeMilli;
    const maxScaleFactor = currentWidthMilli / minTimeMilli;

    const zoom = d3.zoom()
      // .scaleExtent([0.001, 5000])
      .scaleExtent([minScaleFactor, maxScaleFactor])
      .on("zoom", () => {
        this._handleZoom(d3.event.transform);
      });

    this._zoomButtons = new ZoomButtons(plotContainer, listenerRect, zoom);

    listenerRect.call(zoom);
  }

  getSvg(){
    return this.svg;
  }

  _generateCommandsPerSession(commands) {
    const assignParallelCmdCounts = (commandsPerSession) => {
      // find out the number of parallel commands in each session and store it 
      // in the meta-info of each cmd. The groups are already assigned, one command
      // is parallel to another, if there exists at least one command
      // between two zero-group-commands. Note that the groups of
      // those in-between-commands may rise and fall arbitrarily often,
      // so keep track of the max.
      commandsPerSession.forEach((session) => {
        // index in the sessions cmd-array, where the last group 0 was seen
        let lastZeroGroupIdx = 0;
        let lastHighestGroup = 0;
        // yes, <= to simplify handling the final command
        for (let i = 1; i <= session.getCmdsWithMeta().length; i++) {
          if (i >= session.getCmdsWithMeta().length || 
              session.getCmdsWithMeta()[i].getGroup() === 0) {
            // a new group has started or we are at end. Assign the found number of parallel
            // commands to all affected commands:
            const countOfParallelCmds = lastHighestGroup + 1; // zero based..

            for (let j = lastZeroGroupIdx; j < i; j++) {
              session.getCmdsWithMeta()[j].setCountOfParallelGroups(countOfParallelCmds);
            }
            // also keep track of the max number of parallel commands in this session
            // for later use
            session.setMaxCountOfParallelCommands(
              Math.max(session.getMaxCountOfParallelCommands(), countOfParallelCmds)
            );

            lastZeroGroupIdx = i;
            lastHighestGroup = 0;
          } else {
            // keep track of the highest group
            lastHighestGroup = Math.max(lastHighestGroup, 
              session.getCmdsWithMeta()[i].getGroup());
          }
        }
      });
    };

    const commandsPerSession = new MapExtended();

    commands.forEach( (cmd) => {
      // note: Map()' iteration order is the insert order, which is
      // desired here -> since the command-array is ordered by startDateTime,
      // the generated session map is also ordered by startDateTime
      const session = commandsPerSession.getDefault(cmd.sessionUuid, 
        () => { return new session_timeline_Session(); });
      session.addCmd(cmd);
    });

    assignParallelCmdCounts(commandsPerSession);

    // assign a group to each session
    const sessionGrpFind = new timeline_group_find_TimelineGroupFind();
    let maxGroup = 0;
    commandsPerSession.forEach((session) => {
      const group = sessionGrpFind.findNextFreeGroup(session.getSessionStartDate(),
        session.getSessionEndDate());

      session.setSessionGroup(group);
      maxGroup = Math.max(maxGroup, group);
    });

    // generate an array of an array of sessions, so all sessions which have
    // the same group are in one array (in correct order).
    // That way one 'line' of sessions can be 
    // drawn easily.
    const groupedSessions = new Array(maxGroup + 1);
    for (let i = 0; i < groupedSessions.length; i++) {
      groupedSessions[i] = [];
    }
    commandsPerSession.forEach( (session) => {
      groupedSessions[session.getSessionGroup()].push(session);
    });

    return groupedSessions;
  }


  /**
   * @return {int} max y offset of the plot
   * @param {*} groupedSessions 
   */
  _prerenderSessions(groupedSessions){
    const ANNOTATION_AND_PADDING = this.annotationDistance + 
      this.annotationCharHeight * 1.5; // * 1.5 -> give some more space

    const _prerenderCmd = (cmdWithMeta, currentOffset, sessionHeight) => {
      if(cmdWithMeta.getCountOfParallelGroups() === 1){
        // non-parallel commands are aligned to session center:
        cmdWithMeta.setHeight(this.sessionBaseHeight);
        const y = currentOffset + sessionHeight/2 - this.sessionBaseHeight/2;
        cmdWithMeta.setY(y);
        return;
      }
      // parallel commands expand in equal parts over the whole sessionHeight 
      // (separated by padding)
      let cmdHeight = sessionHeight / cmdWithMeta.getCountOfParallelGroups();
      if(cmdHeight < this.sessionMinHeight){
        cmdHeight = this.sessionMinHeight;
      } else {
        cmdHeight -= this.sessionPadding;
      }
      cmdWithMeta.setHeight(cmdHeight);
      const y = currentOffset + (cmdHeight + this.sessionPadding) * cmdWithMeta.getGroup();
      cmdWithMeta.setY(y);
    };

    let currentOffset = 0;
    groupedSessions.forEach((sessionLine, lineIdx) => {
      // find the max. number of parallel commands in all sessions of the current line:
      const maxNumberOfParallelCmds = sessionLine.reduce((prev, curr) => {
        return prev.getMaxCountOfParallelCommands() > curr.getMaxCountOfParallelCommands() ?
         prev : curr;
      }).getMaxCountOfParallelCommands();    
      const sessionHeight = maxNumberOfParallelCmds === 1 ?
       this.sessionBaseHeight :
       (this.sessionMinHeight + this.sessionPadding) * maxNumberOfParallelCmds;

      sessionLine.forEach((session) => {
        session.getCmdsWithMeta().forEach((cmdWithMeta) => {
          _prerenderCmd(cmdWithMeta, currentOffset, sessionHeight);
        });
        session.setHeight(sessionHeight);
        session.setY(currentOffset);
      });
      currentOffset += sessionHeight + ANNOTATION_AND_PADDING;
    });    
    return currentOffset;    
  }

  _preRenderAnnotations(groupedSessions){
    groupedSessions.forEach((sessionLine) => { 
      const annotationGroup = [];
      sessionLine.forEach((session) => {
        session.getCmdsWithMeta().forEach((cmdWithMeta) => {
          // only create annotations for the topmost commandgroup 
          // (in case of parallel commands)
          if(cmdWithMeta.getCountOfParallelGroups() === cmdWithMeta.getGroup() + 1){
            annotationGroup.push(this._createAnnotation(cmdWithMeta, 
              session.getY() + session.getHeight() + this.annotationDistance ));
          }
        });
      });
      this._annotationRender.addAnnotationGroup(annotationGroup);
    });    
  }
  

  _createAnnotation(cmdWithMeta, y){
    return {
      data: cmdWithMeta,
      note: {
        align: "left", 
        wrap: 'nowrap',
        // title: "Annotation title"
      },
      dx: 0,
      ny: this.svgHeight - y,
      y: this.svgHeight - (cmdWithMeta.getY() + cmdWithMeta.getHeight()),
      startX: cmdWithMeta.cmd.startTime,
      endX: cmdWithMeta.cmd.endTime,
      fulltext: cmdWithMeta.cmd.command,
    };
  }


  _calcRectXPosition(cmd, xScale) {
    let startX = xScale(cmd.startTime);
    const w = xScale(cmd.endTime) - startX;
    if (w < this.CMD_MIN_WIDTH) {
      // since a cmd has to have at least that width, but shall be
      // centered anyway:
      const center = startX + w / 2.0;
      startX = center - this.CMD_MIN_WIDTH / 2.0;
    }
    return startX;
  }


  _calcRectWidth(cmd, xScale) {
    const w = xScale(cmd.endTime) - xScale(cmd.startTime);
    if (w < this.CMD_MIN_WIDTH) {
      return this.CMD_MIN_WIDTH;
    }
    return w;
  }


  _handleZoom(transform) {
    const xScaleNew = transform.rescaleX(this.xScale);

    this.axisBottom.scale(xScaleNew);
    this.xAxisDraw.call(
      this.axisBottom
      // .ticks(d3.timeWeek, 2)
      // .tickFormat(d3.timeFormat('%b %d'))
    );
    // maybe_todo: execute in parallel...
    this.commandRects.forEach((rectGroup) => {
      rectGroup.attr('x', (cmdWithMeta) => {
        const pos = this._calcRectXPosition(cmdWithMeta.cmd, xScaleNew);
        // note: pos may be less than zero which is ok, because
        // otherwise wide rects may disappear too soon.
        return pos;
        })
        .attr('width', (cmdWithMeta) => {
          return this._calcRectWidth(cmdWithMeta.cmd, xScaleNew);
        });
    });

    this._annotationRender.update(xScaleNew);
  }
}


class _CommandWithMeta{
  /**
   * 
   * @param {Command} cmd 
   * @param {int} group the group assigned within a session
   */
  constructor(cmd, group){
    this.cmd = cmd;
    this._group = group;
    this._countOfParallelGroups = -1;
    this._height = 1000;
    this._y = 0;
    this._annotation = null;
  }

  getGroup(){
    return this._group;
  }

  setCountOfParallelGroups(val){
    this._countOfParallelGroups = val;
  }

  getCountOfParallelGroups(){
    return this._countOfParallelGroups;
  }

  setHeight(val){
    this._height = val;
  }

  getHeight(){
    return this._height;
  }

  setY(val){
    this._y = val;
  }

  getY(){
    return this._y;
  }

  setAnnotation(val){
    this._annotation = val;
  }

  getAnnotation(){
    return this._annotation;
  }

}

class session_timeline_Session {
  constructor() {
    this._cmdsWithMeta = [];
    this._finalCmdEndDate = DATE_MIN;
    this._groupFind = new timeline_group_find_TimelineGroupFind();
    this._firstCmdStartDate = null;
    this._sessionGroup = null;
    this._maxCountOfParallelCmds = null;
    this._height = null;
  }

  /**
   * The passed commands *must* be sorted (asc) by startTime during
   * subsequent calls of this method.
   * @param {Command} cmd 
   */
  addCmd(cmd) {
    if(this._firstCmdStartDate === null){
      // commands are sorted by startTime and we are called the first time.
      this._firstCmdStartDate = cmd.startTime;
    }
    // commands are sorted by startTime but the first executed cmd may well finish
    // last, so incrementally find the final endDate.
    this._finalCmdEndDate = date_max(cmd.endTime, this._finalCmdEndDate);

    const group = this._groupFind.findNextFreeGroup(cmd.startTime, cmd.endTime);
    this._cmdsWithMeta.push(new _CommandWithMeta(cmd, group));

  }

  setMaxCountOfParallelCommands(val){
    this._maxCountOfParallelCmds = val;
  }

  getMaxCountOfParallelCommands(){
    return this._maxCountOfParallelCmds;
  }


  getSessionStartDate(){
    return this._firstCmdStartDate;
  }

  getSessionEndDate(){
    return this._finalCmdEndDate;
  }

  setSessionGroup(val){
    this._sessionGroup = val;
  }

  getSessionGroup(){
    return this._sessionGroup;
  }

  getCmdsWithMeta(){
    return this._cmdsWithMeta;
  }

  setHeight(val){
    this._height = val;
  }

  getHeight(){
    return this._height;
  }

  setY(val){
    this._y = val;
  }

  getY(){
    return this._y;
  }


}

// CONCATENATED MODULE: ./src/d3js_util.js


/**
 * Wrap long axis labels to mutliple lines by maximum width, splitting words by 
 * the given *delimeter-keeping* splitStr and auto-truncating long words.
 * Leading and trailing whitespaces of each line are trimmed.
 * @param {[String]} tickTexts 
 * @param {int} width The max. width in pixels for each label 
 * @param {RegExp} splitStr A regular expression for the split-string, 
 * which keeps the delimeter, e.g. /(?=\s)/.
 */
function wrapTextLabels(tickTexts, width, splitStr=/(?=\s)/) {
  tickTexts.each(function() {
    const text = d3.select(this);
    const words = text.text().split(splitStr);
    let line = [];
    let lineNumber = 0;
    const lineHeight = 1.1; // ems
    const y = text.attr("y");
    const dy = parseFloat(text.attr("dy"));
    let tspan = text.text(null).append("tspan").attr("x", 0).attr("y", y)
      .attr("dy", dy + "em");
    // only increment i, if the word can be for sure drawn to current line
    for(let i=0; i < words.length; ) {
      line.push(words[i]);
      tspan.text(line.join(''));
      if (tspan.node().getComputedTextLength() <= width) {
        ++i;
        continue;
      }

      if (line.length === 1) {
        // this single word is too long to fit into a line -> clip it
        _truncateAndSetLabelTxt(line[0].trim(), tspan, width);
        ++i;    
      } else {
        // this word does not fit any more -> put it to next line 
        // and render all others
        line.pop();
        tspan.text(line.join('').trim());
        
        // NO ++i -> the current word must be rendered in next line
      } 
      tspan = text.append("tspan").attr("x", 0).attr("y", y)
        .attr("dy", `${++lineNumber * lineHeight + dy}em`).text(null);
      line = [];
    }
  });
}


function _truncateAndSetLabelTxt(labelTxt, tspan, width) {
  do {
    labelTxt = labelTxt.slice(0, -3);
    tspan.text(labelTxt);
  } while (tspan.node().getComputedTextLength() > width);
  labelTxt = labelTxt.slice(0, -2);
  labelTxt += '..';
  tspan.text(labelTxt);
}

// CONCATENATED MODULE: ./src/plot_simple_bar.js





/**
 * Base class for several bar plots
 */
class plot_simple_bar_PlotSimpleBar {
  constructor() {
    this._maxCountOfBars = 5;

    this._margin = { top: 20, right: 20, bottom: 60, left: 40 };
    this._width = 500 - this._margin.left - this._margin.right;
    this._height = 300 - this._margin.top - this._margin.bottom;

    this._maxBarWidth = 30;
  }

  setMaxCountOfBars(val){
    this._maxCountOfBars = val;
  }

  generatePlot(data, siblingElement) {
    const plotContainer = siblingElement.append('div')
      .style('position', 'relative')
      .style('padding', '12px')
      .style('display', 'inherit');
      

    this._svg = plotContainer.append("svg")
      .attr("width", this._width + this._margin.left + this._margin.right)
      .attr("height", this._height + this._margin.top + this._margin.bottom)
      .append("g")
      .attr("transform",
        "translate(" + this._margin.left + "," + this._margin.top + ")");

    // chart title
    const chartTitle = this._svg.append("text")
      .attr("x", (this._width / 2.0))
      .attr("y", -3)
      .attr("text-anchor", "middle")
      .style("font-size", "16px")
      .style("text-decoration", "underline")
      .text(this._chartTitle());

    this._xScaleBand = d3.scaleBand()
      .range([0, this._width])
      .padding(0.1);
    this._yScaleBand = d3.scaleLinear()
      // leave some space for the char title:
      .range([this._height, chartTitle.node().getBoundingClientRect().height * 1.2]);

    // In case of duplicate x-axis label values they are overridden, which should
    // never be desired. Instead build a range and access the respective data-array-element
    // by index.
    this._xScaleBand.domain(d3.range(data.length));
    this._yScaleBand.domain(this._yScaleBandDomain());

    const actualBandWidth = (this._xScaleBand.bandwidth() > this._maxBarWidth) ? 
      this._maxBarWidth :
      this._xScaleBand.bandwidth();


    // append the rectangles for the bar chart

    const dataEnterSelection = this._svg.selectAll(".bar").data(data).enter();

    const bars = dataEnterSelection.append("rect")
      .style('fill',(d, i) => { return this._barColor(d); })
      .attr("x", (d, i) => { 
        let x = this._xScaleBand(i);
        const center = x + this._xScaleBand.bandwidth()/2.0;
        x = center - actualBandWidth/2.0;
        return x;
       })
      .attr("width", actualBandWidth) 
      .attr("y", (d) => { return this._yScaleBand(this._yValue(d)); })
      .attr("height", (d) => { return this._height - this._yScaleBand(this._yValue(d)); })
      .attr('data-toggle', 'tooltip')
      .attr('title', (d) => { return this._barTooltipTxt(d); });
  
    this._modifyBars(bars);
      
    // add the x Axis
    this._svg.append("g")
      .attr("transform", "translate(0," + this._height + ")")
      .call(d3.axisBottom(this._xScaleBand).tickFormat((d,i)=> this._xValue(data[i])))
      .selectAll("text")
      .call((tickTexts) => {
        const thisPlot = this;
        tickTexts.each(function (plainTxt, idx) {
          const text = d3.select(this);
          text.attr("title", function () {
            return thisPlot._xAxisTooltipTxt.call(thisPlot, data[idx]);
          }).attr('data-toggle', 'tooltip')
            .attr('data-placement', 'left');
          thisPlot._modifyTickText(text, data[idx]);  
        });

        wrapTextLabels(tickTexts, 
          this._xScaleBand.bandwidth(), 
          this._xTxtLabelSplitStr());  
      });

    // add the y Axis
    const yAxisTicks = this._yScaleBand.ticks()
      .filter((tick) => { return this._yAxisTicksFilter(tick); });
    this._yaxis = d3.axisLeft(this._yScaleBand);
    const yTickFormat = this._yAxisTickFormat();
    if(yTickFormat !== undefined){
      this._yaxis.tickValues(yAxisTicks).tickFormat( yTickFormat );
    }

    this._svg.append("g").call(this._yaxis);      
  }

  
  // MUST override methods
  _chartTitle(){ throw new ErrorNotImplemented(); }
  _yScaleBandDomain(){ throw new ErrorNotImplemented(); }
  // Is called for each x-value
  _xValue(d){ throw new ErrorNotImplemented(); }
  // Is called for each y-value
  _yValue(d){ throw new ErrorNotImplemented(); }

  // MAY override methods
  _yAxisTicksFilter(tick){ return true; }
  _yAxisTickFormat() { return undefined; }
  _modifyTickText(tickTxt, data) {}

  _xTxtLabelSplitStr() { return /(?=\s)/; }  
  _barTooltipTxt(dataElement){
    return this._xValue(dataElement);
  }
  _xAxisTooltipTxt(dataElement){
    return this._xValue(dataElement);
  }
  _barColor(dataElement){
    return 'steelblue';
  }
  // apply further modifications to the bars
  _modifyBars(bars){}

}


// CONCATENATED MODULE: ./src/plot_most_written_files.js






/**
 * A bar plot displaying the commands which
 * modified the most files.
 */
class plot_most_written_files_PlotMostWrittenFiles extends plot_simple_bar_PlotSimpleBar {

  generatePlot(commands, siblingElement){
    this._filteredCmds = [];
    mostFileMods.forEach((e) => {
      this._filteredCmds.push(commands[e.idx]);
    });
    this._maxCountOfWfileEvents = this._filteredCmds[0].fileWriteEvents_length;

    // Be consistent with timeline and sort by date:
    this._filteredCmds.sort(compareStartDates);

    super.generatePlot(this._filteredCmds, siblingElement);

  }
  /**
   * @override
   */
  _chartTitle(){ return 'Commands with most file-modifications'; }

  /**
   * @override
   */  
  _yScaleBandDomain(){ return [0, this._maxCountOfWfileEvents]; }

  /**
   * @override
   */  
  _xValue(cmd) {
    return humanDateFormatOnlyDate(cmd.startTime) + ": " +
      cmd.command;
  }  

  /**
   * @override
   */  
  _yValue(cmd) {
    return cmd.fileWriteEvents_length;
  }  

  /**
   * @override
   */  
  _yAxisTicksFilter(tick){ return Number.isInteger(tick); }

  /**
   * @override
   */  
  _yAxisTickFormat() { return d3.format('d'); }


  _barColor(cmd){
    return cmd.sessionColor;
  }

  _modifyBars(bars){
    bars
      .style('cursor', 'pointer')
      .on("click", (cmd) => { 
        commandList.scrollToCmd(cmd); 
      });
  }

  _modifyTickText(tickTxt, cmd) {
    tickTxt
      .style('cursor', 'pointer')
      .on("click", () => { 
        commandList.scrollToCmd(cmd); 
      });
  }
}



// CONCATENATED MODULE: ./src/plot_cmdcount_per_cwd.js




/**
 * A bar plot displaying the working directories
 * where the most commands were executed.
 */
class plot_cmdcount_per_cwd_PlotCmdCountPerCwd extends plot_simple_bar_PlotSimpleBar {

  generatePlot(commands, siblingElement){
    super.generatePlot(cwdCmdCounts, siblingElement);

  }
  /**
   * @override
   */
  _chartTitle(){ return 'Working directories with most commands'; }


  /**
   * @override
   */  
  _yScaleBandDomain(){ return [0, cwdCmdCounts[0].countOfCommands]; }

  /**
   * @override
   */  
  _xValue(cwdCmdCount) {
    return cwdCmdCount.workingDir;
  }  

  /**
   * @override
   */  
  _yValue(cwdCmdCount) {
    return cwdCmdCount.countOfCommands;
  }  

  /**
   * @override
   */  
  _yAxisTicksFilter(tick){ return Number.isInteger(tick); }

  /**
   * @override
   */  
  _yAxisTickFormat() { return d3.format('d'); }


  _xTxtLabelSplitStr() { return /(?=\/)/; }

}



// CONCATENATED MODULE: ./src/plot_io_per_dir.js




/**
 * A bar plot displaying directories
 * with most IO-activity.
 */
class plot_io_per_dir_PlotIoPerDir extends plot_simple_bar_PlotSimpleBar {

  generatePlot(commands, siblingElement){
    super.generatePlot(dirIoCounts, siblingElement);

  }
  /**
   * @override
   */
  _chartTitle(){ return 'Directories with most input-output-activity'; }


  /**
   * @override
   */  
  _yScaleBandDomain(){ return [0, dirIoCounts[0].readCount + dirIoCounts[0].writeCount]; }

  /**
   * @override
   */  
  _xValue(ioStat) {
    return ioStat.dir;
  }  

  /**
   * @override
   */  
  _yValue(ioStat) {
    return ioStat.readCount + ioStat.writeCount;
  }  

  /**
   * @override
   */  
  _yAxisTicksFilter(tick){ return Number.isInteger(tick); }

  /**
   * @override
   */  
  _yAxisTickFormat() { return d3.format('d'); }
  
  /**
   * @override
   */  
  _xTxtLabelSplitStr() { return /(?=\/)/; }
}

// CONCATENATED MODULE: ./src/plot_cmdcount_per_session.js





/**
 * A bar plot displaying the sessions
 * wherein the most commands were executed.
 */
class plot_cmdcount_per_session_PlotCmdCountPerSession extends plot_simple_bar_PlotSimpleBar {

  generatePlot(commands, siblingElement) {
      this._sessionMostCmds = [];
      sessionsMostCmds.forEach((e) => {
        this._sessionMostCmds.push(
          new _SessionMostCmdsEntry(commands[e.idxFirstCmd], e.countOfCommands)
          );
      });
      this._maxCountOfCmdsInSession = this._sessionMostCmds[0].countOfCommands;
    
    // sort the sessions by start date
    this._sessionMostCmds.sort((s1, s2) => {
      return s1.firstCmd.startTime - s2.firstCmd.startTime;
    });

    super.generatePlot(this._sessionMostCmds, siblingElement);

  }
  /**
   * @override
   */
  _chartTitle(){ return 'Sessions with most commands'; }


  /**
   * @override
   */  
  _yScaleBandDomain(){ return [0, this._maxCountOfCmdsInSession]; }

  /**
   * @override
   */  
  _xValue(session) {
    return session.firstCmd.sessionUuid;
  }  

  /**
   * @override
   */  
  _yValue(session) {
    return session.countOfCommands;
  }  

  /**
   * @override
   */  
  _yAxisTicksFilter(tick){ return Number.isInteger(tick); }

  /**
   * @override
   */  
  _yAxisTickFormat() { return d3.format('d'); }


  /**
   * @return {int}
   * @param {[Command]} cmds1 
   * @param {[Command]} cmds2 
   */
  _compareBySessionCmdCount(cmds1, cmds2) {
    return cmds1.length - cmds2.length;
  }

  _barColor(session){
    return session.firstCmd.sessionColor;
  }

  _modifyBars(bars){
    bars
      .style('cursor', 'pointer')
      .on("click", (session) => { 
        commandList.scrollToCmd(session.firstCmd); 
      });
  }

  _modifyTickText(tickTxt, session) {
    tickTxt
      .style('cursor', 'pointer')
      .on("click", () => { 
        commandList.scrollToCmd(session.firstCmd); 
      });
  }

}


class _SessionMostCmdsEntry {
  constructor(firstCmd, countOfCommands){
    this.firstCmd = firstCmd;
    this.countOfCommands = countOfCommands;
  }
}



// CONCATENATED MODULE: ./src/stats.js








async function generateMiscStats() {
  const body = d3.select('body');

  if (typeof commands[0].fileWriteEvents === 'undefined') {
    // when generating from shournal, command-data (like fileWriteEvents)
    // is loaded later for performance reasons
    await timedForEach(commands, (cmd, idx) => {
      const cmdDataTag = d3.select('#commandDataJSON' + idx);
      const cmdData = JSON.parse(cmdDataTag.html());
      Object.assign(cmd, cmdData);
      cmdDataTag.remove();
    });
  }

  if (commands.length < 5) {
    // statistics are boring with so few commands...
    return;
  }

  body.append('h3')
    .html('Miscellaneous statistics')
    .style('padding-top', '1em');

  const miscStatElement = body.append('div')
    .style('padding-top', '20px')
    .style('display', 'inline-block');

  const plotMostWrittenFiles = new plot_most_written_files_PlotMostWrittenFiles();
  plotMostWrittenFiles.generatePlot(commands, miscStatElement);

  const plotCmdCountPerSession = new plot_cmdcount_per_session_PlotCmdCountPerSession();
  plotCmdCountPerSession.generatePlot(commands, miscStatElement);

  const plotCmdCountPerCwd = new plot_cmdcount_per_cwd_PlotCmdCountPerCwd();
  plotCmdCountPerCwd.generatePlot(commands, miscStatElement);

  const plotIoPerDir = new plot_io_per_dir_PlotIoPerDir();
  plotIoPerDir.generatePlot(commands, miscStatElement);


  $('[data-toggle="tooltip"]').tooltip({
    delay: { show: 300, hide: 0 },
  });
}

// CONCATENATED MODULE: ./src/index.js








function displayErrorAtTop(msg){
  // vanilla js, since loading of libraries might have failed
  const errEl = document.getElementById('topError');
  errEl.style["visibility"] = "visible";
  errEl.innerHTML = msg;
}


function main() {
  if (scriptLoadError) {
    console.log(scriptLoadError);
    displayErrorAtTop(scriptLoadError);
    return;
  }

  init();

  assert(commands.length > 0, 'commands.length > 0');

  const queryDate = d3TimeParseIsoWithMil(ORIGINAL_QUERY_DATE_STR);
  const body = d3.select('body');

  body.append('button')
    .attr('class', 'btn btn-primary')
    .style('position', 'absolute')
    .style('right', '0px')
    .style('top', '0px')
    .html("Report Metadata")
    .on("click", () => {
      textDialog.show("Report Metadata", 
        `Commandline-query (executed on ` + 
        `${humanDateFormat(queryDate)}): ${ORIGINAL_QUERY}`);
    });

  prepareCommands(commands);

  {
    let lastStart = commands[0].startTime;
    for(let i=1; i < commands.length; i++){
      assert(commands[i].startTime >= lastStart);
      lastStart = commands[i].startTime;
    }
  }
  

  const cmdFinalEndDate = d3TimeParseIsoWithMil(CMD_FINAL_ENDDATE_STR);
 
  // Do not change order -> commandList.size computed based on sessionTimeLine.size.
  sessionTimeline = new session_timeline_SessionTimeline(commands, cmdFinalEndDate);
  commandList = new command_list_CommandList(commands);
  
  d3.select('#initialSpinner').remove();
  $(document).ready(generateMiscStats);
}


try {
  main();
} catch (error) {
  console.log(error);
  displayErrorAtTop(error);
}


/***/ })
/******/ ]);