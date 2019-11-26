
import PlotSimpleBar from './plot_simple_bar';

import * as command_manipulation from './command_manipulation';
import * as globals from './globals';

/**
 * A bar plot displaying the commands which
 * modified the most files.
 */
export default class PlotMostWrittenFiles extends PlotSimpleBar {

  generatePlot(commands, siblingElement){
    this._filteredCmds = [];
    mostFileMods.forEach((e) => {
      this._filteredCmds.push(commands[e.idx]);
    });
    this._maxCountOfWfileEvents = this._filteredCmds[0].fileWriteEvents_length;

    // Be consistent with timeline and sort by date:
    this._filteredCmds.sort(command_manipulation.compareStartDates);

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
    return globals.humanDateFormatOnlyDate(cmd.startTime) + ": " +
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
        globals.commandList.scrollToCmd(cmd); 
      });
  }

  _modifyTickText(tickTxt, cmd) {
    tickTxt
      .style('cursor', 'pointer')
      .on("click", () => { 
        globals.commandList.scrollToCmd(cmd); 
      });
  }
}


