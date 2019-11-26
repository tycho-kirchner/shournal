

import PlotSimpleBar from './plot_simple_bar';

/**
 * A bar plot displaying directories
 * with most IO-activity.
 */
export default class PlotIoPerDir extends PlotSimpleBar {

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
