import * as html_util from './html_util';
import * as util from './util';
import * as globals from './globals';
import * as conversions from './conversions';


export default class CommandList {
  constructor(commands) {

    this._CMDLISTPADDING = 18;
    this._CMDLISTBG = '#777';

    const cmdListHeight = (() => {
      const boundClient = globals.sessionTimeline.getSvg().node().getBoundingClientRect();
      let h = util.windowHeight() - (boundClient.y + boundClient.height) - 30; // why minus 30?
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
        ) ? globals.humanDateFormatOnlyTime : globals.humanDateFormat;
        return globals.humanDateFormat(cmd.startTime) + ' - ' +
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
          return `${e.path} (${conversions.bytesToHuman(e.size)}), Hash: ${e.hash}`;
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
      .text((e) => { return `${e.path} (${conversions.bytesToHuman(e.size)}), Hash: ${e.hash}`; })
      .on("click", (readFile) => {
        if (readFile.isStoredToDisk) {
          const mtimeHuman = globals.humanDateFormat(d3.isoParse(readFile.mtime));
          const title = `Read file ${readFile.path}<br>` +
                        `mtime: ${mtimeHuman}<br>` +
                        `size: ${conversions.bytesToHuman(readFile.size)}<br>` + 
                        `hash: ${readFile.hash}<br>`;

          const readFileContent = atob(readFileContentMap.get(readFile.id));
          globals.textDialog.show(title, readFileContent);
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
    html_util.insertAfter(contentDiv.node(), cmdElement.node());

    const cmdListScroll = document.getElementById('cmdListScroll');
    if(! html_util.isScrolledIntoView(contentDiv.node(), cmdListScroll, true)){
      // scroll down one element, so at least the beginning of content is visible:
      cmdListScroll.scrollTop += cmdElement.node().clientHeight;  
    } 
  }
}
