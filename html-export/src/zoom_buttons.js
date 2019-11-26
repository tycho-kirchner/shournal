

export default class ZoomButtons {
  
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
