

import PlotSimpleBar from './plot_simple_bar';
import * as globals from './globals';

/**
 * A bar plot displaying the sessions
 * wherein the most commands were executed.
 */
export default class PlotCmdCountPerSession extends PlotSimpleBar {

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
        globals.commandList.scrollToCmd(session.firstCmd); 
      });
  }

  _modifyTickText(tickTxt, session) {
    tickTxt
      .style('cursor', 'pointer')
      .on("click", () => { 
        globals.commandList.scrollToCmd(session.firstCmd); 
      });
  }

}


class _SessionMostCmdsEntry {
  constructor(firstCmd, countOfCommands){
    this.firstCmd = firstCmd;
    this.countOfCommands = countOfCommands;
  }
}


