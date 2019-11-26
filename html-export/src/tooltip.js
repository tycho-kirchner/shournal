

export default class Tooltip {

  constructor(){
    this._tooltipDiv = d3.select('body')
    .append('div')
    .style("position", "absolute")
    .style("visibility", 'hidden')
    .style("background-color", "white")
    .style("border", "solid")
    .style("border-width", "2px")
    .style("border-radius", "5px")
    .style("padding", "5px")
    .style("z-index", "1000")
    .style("pointer-events", "none"); // no flickering in chromium...
  }

  show(txt, x, y) {
    // maybe_todo: if tooltip is too much on the right, it gets clipped. Maybe use solution from
    // https://stackoverflow.com/a/51066294/7015849
    this._tooltipDiv
      .style("left", x + "px")
      .style("top", y + "px")
      .style('visibility', 'visible')
      .html(txt);
  }

  hide() {
    this._tooltipDiv.style('visibility', 'hidden');
  }

}
