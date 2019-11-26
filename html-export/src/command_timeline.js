import * as util from './util';
import Tooltip from './tooltip';
import AnnotationLineRender from './annotation_line_render';

export default class CommandTimeline {
  constructor(commands, countOfCmdGroups,
    cmdFinalEndDate) {
    this.commands = commands;
    this.countOfCmdGroups = countOfCmdGroups;
    this.cmdFinalEndDate = cmdFinalEndDate;

    this._margin = {
      top: 20,
      right: 20,
      bottom: 24,
      left: 24,
    };

    this.commandsPerGroup = this._generateCommandsPerGroup();

    // get the width in pixel of a character
    this.annotationCharWidth = d3.select("#annotation_text_char").node()
      .getBoundingClientRect().width;
    this.annotationCharHeight = d3.select("#annotation_text_char").node()
      .getBoundingClientRect().height;

    // An annotation shall only be displayed, if its minimum width in pixel
    // is at least 5 character. Warning: do not set < 1 -> text rendering issues for annotations
    // distance to the belonging command rect
    this.ANNOTATION_DISTANCE = 15;

    // minimum width of a cmd-rect. Let it be at least 1, otherwise very short commands
    // are barely visible (get another color...)
    this.CMD_MIN_WIDTH = 4;

    this.TOTAL_CMD_GROUP_HEIGHT = _CmdRectHeights.VERY_MANY_MOD + this.ANNOTATION_DISTANCE +
        this.annotationCharHeight * 2; // *2 to give some more space

    this.cmdGroupOffsets = this._generateCommandGroupOffsets();

    this.svgWidth = util.windowWidth() - this._margin.left - this._margin.right - 30;
    // min. height, might be increased below
    this.svgHeight = 100;
    // If too many command-groups, increase plot size
    if (this.svgHeight < this.cmdGroupOffsets[this.cmdGroupOffsets.length - 1] + this.TOTAL_CMD_GROUP_HEIGHT) {
      this.svgHeight = this.cmdGroupOffsets[this.cmdGroupOffsets.length - 1] + this.TOTAL_CMD_GROUP_HEIGHT;
    }

    this.xScale = d3.scaleTime()
      .range([0, this.svgWidth]);

    this.axisBottom = d3.axisBottom(this.xScale);

    this.svg = d3.select('body').append('svg')
       .attr('width', this._margin.left + this.svgWidth + this._margin.right)
       .attr('height', this._margin.top + this.svgHeight + this._margin.bottom)
      .append('g')
      .attr('transform', 'translate(' + this._margin.left + ',' + this._margin.top + ')')
      .style('z-index', -1);

    this._annotationRender = new AnnotationLineRender(this.svg);

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
      this.commands[0].startTime,
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

    this.tooltip = new Tooltip();

    // draw rectangles
    this.commandRects = this.svg.selectAll('rect')
      .data(commands)
      .enter()
      .append('rect')
      .attr('x', (cmd) => { return this._calcRectXPosition(cmd, this.xScale); })
      .attr('y', (cmd) => { return this.svgHeight - this.cmdGroupOffsets[cmd.vertOffsetGroup]; })
      .attr('width', (cmd) => { return this._calcRectWidth(cmd, this.xScale); })
      .attr('height', (cmd) => {
        if(cmd.fileWriteEvents.length === 0) return _CmdRectHeights.NO_MOD;
        if(cmd.fileWriteEvents.length < 5) return _CmdRectHeights.FEW_MOD;
        if(cmd.fileWriteEvents.length < 15) return _CmdRectHeights.MANY_MOD;
        return _CmdRectHeights.VERY_MANY_MOD;
      })
      .attr('fill', (cmd, i) => {
        return cmd.sessionColor;
        //  maybe_todo: mark 'sessionEnd' with a color?
        // const p = 0.1 * 100;
        // const grad = defs.append("linearGradient")
        //     .attr("id", "grad_" + i);
        // 
        // const color1 = "orange";
        // const color2 = "steelblue";
        // 
        // grad.append("stop")
        //   .attr("offset", "0%")
        //   .attr("stop-color", color1);
        // grad.append("stop")
        //   .attr("offset", (p) + "%")
        //   .attr("stop-color", color1);
        // grad.append("stop")
        //   .attr("offset", (p) + "%")
        //   .attr("stop-color", color2);
        // grad.append("stop")
        //   .attr("offset", "100%")
        //   .attr("stop-color", color2);
        // 
        // return "url(#grad_" + i + ")";
      })
      .on("mouseover", (cmd) => { 
        this.tooltip.show(cmd.command, d3.event.pageX, d3.event.pageY);
       })
      .on("mouseout", () => { this.tooltip.hide(); })
      .on("click", (cmd) => { this._commandList.scrollToCmd(cmd); });

    this._setupAnnotations();

    const zoom = d3.zoom()
      .scaleExtent([0.001, 5000])
      .on("zoom", () => {
        this._handleZoom(d3.event.transform);
      });
    
    listenerRect.call(zoom);
  }

  getSvg(){
    return this.svg;
  }

  setCommandList(commandList){
    this._commandList = commandList;
  }

  _setupAnnotations(){
    this.commandsPerGroup.forEach((cmdGroup) => {
      const annotationGroup = [];
      cmdGroup.forEach((cmd) => {
        const annotation = {
          data: cmd,
          note: {
            align: "left", 
            wrap: 'nowrap',
            // title: "Annotation title"
          },
          y: this.svgHeight - this.cmdGroupOffsets[cmd.vertOffsetGroup], // TODO: use cmdGroupIdx??
          dx: 0,
          dy: - this.ANNOTATION_DISTANCE,
          startX: cmd.startTime,
          endX: cmd.endTime,
          fulltext: cmd.command,
        };
        annotationGroup.push(annotation);
      });
      this._annotationRender.addAnnotationGroup(annotationGroup);     
    });
    this._annotationRender.setOnNoteOver((cmd) => {
      this.tooltip.show(cmd.command, d3.event.pageX, d3.event.pageY);
    });
    this._annotationRender.setOnNoteOut(() => {
      this.tooltip.hide();
    });
    this._annotationRender.setOnNoteClick((cmd) => {
      this._commandList.scrollToCmd(cmd);
    });

    this._annotationRender.update(this.xScale);
  }


  _generateCommandsPerGroup() {
    // put each cmd-groups into separate arrays:
    const commandsPerGroup = new Array(this.countOfCmdGroups);
    for (let i = 0; i < commandsPerGroup.length; i++) {
      commandsPerGroup[i] = [];
    }
    this.commands.forEach( (cmd) => {
      commandsPerGroup[cmd.vertOffsetGroup].push(cmd);
    });
    return commandsPerGroup;
  }


  _generateCommandGroupOffsets() {
    const offsets = [];
    // dont start directly on the x-axis, but a little higher
    let currentOffset = _CmdRectHeights.VERY_MANY_MOD;
    for (let i = 0; i < this.countOfCmdGroups; i++) {
      offsets.push(currentOffset);
      currentOffset += this.TOTAL_CMD_GROUP_HEIGHT;
    }
    return offsets;
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

    this.commandRects
      .attr('x', (cmd) => {
        const pos = this._calcRectXPosition(cmd, xScaleNew);
        // note: pos may be less than zero which is ok, because
        // otherwise wide rects may disappear too soon.
        return pos;
      })
      .attr('width', (cmd) => { return this._calcRectWidth(cmd, xScaleNew); });

    this._annotationRender.update(xScaleNew); 
  }


}

// TODO: document it
class _CmdRectHeights {
  static get NO_MOD() { return 7; }
  static get FEW_MOD() { return 14; }
  static get MANY_MOD() { return 20; }
  static get VERY_MANY_MOD() { return 24; }
}
