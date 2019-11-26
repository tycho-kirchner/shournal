

import * as d3js_util from './d3js_util';
import {ErrorNotImplemented} from './util';

/**
 * Base class for several bar plots
 */
export default class PlotSimpleBar {
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

        d3js_util.wrapTextLabels(tickTexts, 
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

