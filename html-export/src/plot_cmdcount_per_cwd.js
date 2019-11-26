

import PlotSimpleBar from './plot_simple_bar';

/**
 * A bar plot displaying the working directories
 * where the most commands were executed.
 */
export default class PlotCmdCountPerCwd extends PlotSimpleBar {

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


