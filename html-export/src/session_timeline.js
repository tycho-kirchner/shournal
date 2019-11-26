import * as util from './util';
import MapExtended from './map_extended';
import TimelineGroupFind from './timeline_group_find';
import AnnotationLineRender from './annotation_line_render';
import ZoomButtons from './zoom_buttons';
import * as globals from './globals';

export default class SessionTimeline {
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

    this.svgWidth = util.windowWidth() - this._margin.left - this._margin.right - 30;

    const plotContainer = d3.select('body').append('div')
      .style('position', 'relative'); // see https://stackoverflow.com/a/10487329

    this.svg = plotContainer.append('svg');
    this._annotationRender = new AnnotationLineRender(this.svg);

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
          globals.commandList.scrollToCmd(cmdWithMeta.cmd); 
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
      globals.commandList.scrollToCmd(cmdWithMeta.cmd);
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
        () => { return new _Session(); });
      session.addCmd(cmd);
    });

    assignParallelCmdCounts(commandsPerSession);

    // assign a group to each session
    const sessionGrpFind = new TimelineGroupFind();
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

class _Session {
  constructor() {
    this._cmdsWithMeta = [];
    this._finalCmdEndDate = util.DATE_MIN;
    this._groupFind = new TimelineGroupFind();
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
    this._finalCmdEndDate = util.date_max(cmd.endTime, this._finalCmdEndDate);

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
