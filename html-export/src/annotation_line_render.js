

import * as util from './util';
import {sleep} from './util';
import {Mutex} from 'async-mutex';

/**
 * Render Groups of annotations on a per-line-basis. Clip annotation texts 
 * and omit annotations as needed to fit into available space
 */
export default class AnnotationLineRender {
  constructor(plot) {
    this._annotationGroups = [];
    this._plot = plot;
    // get the width in pixel of a character
    this._annotationCharWidth = d3.select("#annotation_text_char").node()
      .getBoundingClientRect().width;
    // do not render an annotation which does not fit into the space.
    this._annotationMinWidth = this._annotationCharWidth * 2;
    // clip annotation-texts after that many characters
    this._annotationMaxNumChars = 15;
    this._updateMutex = new Mutex();
    this._lastUpdateDummy = null;
  }

  /**
   * 
   * @param {Array<Annotation>} group: ordered set of annotations which will be rendered
   * within the same line. Base class is the same as d3 annotation, however, the following
   * *additional* fields must be set: startX, endX, fulltext. The annotation position
   * (x,y) has to be set already, based on the x-values it is decided, how much of
   * an annotation is drawn.
   */
  addAnnotationGroup(group) {
    this._annotationGroups.push(group);
  }


  async update(xScale) {
    this._lastUpdateDummy = {};
    const currentUpdateDummy = this._lastUpdateDummy;

    const release = await this._updateMutex.acquire();
    try {
      // remove and add again seems to be faster than updating
      this._plot.selectAll('.annotation').remove();
      this._plot.selectAll('.annotationVertLine').remove();
      this._plot.selectAll('.annotationHorizLine').remove();

      const annotations = await this._preRenderAnnotations(xScale, currentUpdateDummy);
      if (annotations !== null) {
        this._appendAnnotations(annotations);
      }
    } finally {
      release();
    }
  } 

  setOnNoteClick(func){
    this._onNoteClick = func;
  }

  // ***************** PRIVATE ********************

  _compareStartX(prev, current) {
    return prev.startX - current.startX;
  }  

  _compareEndX(prev, current) {
    return prev.endX - current.endX;
  }  

  async _preRenderAnnotations(xScale, currentUpdateDummy ) {  
    
    const annotations = [];
    // uniform interface for binary search, where the entrance indeces are found
    const dummyAnnotation = {
      startX: xScale.domain()[0],
      endX: xScale.domain()[1],
    };

    const plotWidth = this._plot.node().getBBox().width;
    for(const annotationLine of this._annotationGroups) {
      if (annotationLine.length == 0) {
        continue;
      }
      // Do not render annotations outside the current view
      // -> find start and stop indeces in the group:
      // Note: one cannot simply choose 0 and length -1 after zooming
      // out, because panning also has to be respected.
      const startIdx = util.binarySearch(annotationLine, dummyAnnotation, 
        this._compareStartX, true);
      const endIdx = util.binarySearch(annotationLine, dummyAnnotation, 
        this._compareEndX, true);

      let displayAnnotation = annotationLine[startIdx];
      displayAnnotation.x = this._calcAnnotationCenter(displayAnnotation, xScale);

      for (let idx = startIdx + 1; idx <= endIdx; idx++) {
        // this.update is run async: check if it was called in between. If that's the
        // case we can abort, because or xScale is outdated.
        if (currentUpdateDummy !== this._lastUpdateDummy) {
          return null;
        }  
        
        if(idx % 30 === 0){
          // avoid freezing the DOM...
          await sleep(5);
        }        

        const annotation = annotationLine[idx];
        annotation.x = this._calcAnnotationCenter(annotation, xScale);

        const textspace = annotation.x - displayAnnotation.x -
          (this._annotationCharWidth * 2); // subtract more chars to leave space to next annotation
        const annotationTxt = this._generateAnnotationTxt(textspace, displayAnnotation.fulltext);
        if (annotationTxt == null) {
          // do not render this annotation
          continue;
        }
        // always update text, we might have zoomed before!
        displayAnnotation.note.label = annotationTxt;
        annotations.push(displayAnnotation);
        displayAnnotation = annotation;
      }

      // still need to push the final annotation, if it fits into our plot
      const textspace = plotWidth - displayAnnotation.x;
      const annotationTxt = this._generateAnnotationTxt(textspace, displayAnnotation.fulltext);
      if (annotationTxt != null) {
        displayAnnotation.note.label = annotationTxt;
        annotations.push(displayAnnotation);
      }
    }
    return annotations;
  }

  _calcAnnotationCenter(annotation, xScale) {
    return (xScale(annotation.startX) + xScale(annotation.endX)) / 2.0;
  }

    /**
   * @param {*} textspace Available width in pixel
   * @param {*} txt The full text
   * @return {*} null, if textspace was too small, else the full or clipped text
   */
  _generateAnnotationTxt(textspace, txt) {
    if (textspace < this._annotationMinWidth) {
      return null;
    }

    // Render only so many chars that fit into the space, but not more than
    // _annotationMaxNumChars;
    const maxCountOfRenderChars = Math.min(Math.ceil(textspace / this._annotationCharWidth) , 
      this._annotationMaxNumChars);
    
    if (txt.length <= maxCountOfRenderChars ) {
      return txt;
    }
    return txt.substring(0, maxCountOfRenderChars - 1) + '.';
  }


  /**
   * Append all annotations to the plot and setup mouse event handlers
   * @param {[annotation]} annotations
   */
  _appendAnnotations(annotations) {
    const enterSelection = this._plot.selectAll(".annotation")
      .data(annotations)
      .enter();

    enterSelection
      .append("text")
      .attr('class', 'annotation unselectable' )
      .attr('x', (a) => { return a.x; })
      .attr('y', (a) => { return a.ny; })
      .text((a) => { return a.note.label; })
      .attr('title', (a) => { return a.fulltext; })
      .style('cursor', 'pointer')
      .on("click", (a) => {
        if (this._onNoteClick !== undefined) {
          // d3.event.pageX, d3.event.pageY
          this._onNoteClick(a.data);
        }
      });

    // dynamically inserted elements -> rerun tooltip
    $('.annotation').tooltip({
      delay: { show: 100, hide: 0 },
    });

    const horzLineYOffset = 2;  

    const lineColor = 'steelblue';

    enterSelection
      .insert("line")
      .attr('class', 'annotationVertLine')
      .attr('x1', (a) => { return a.x; })
      .attr('y1', (a) => { return a.ny + horzLineYOffset; })
      .attr('x2', (a) => { return a.x; })
      .attr('y2', (a) => { return a.y; })
      .attr("stroke-width", 0.5)
      .attr("stroke", lineColor);
      
    enterSelection
      .insert("line")
      .attr('class', 'annotationHorizLine')
      .attr('x1', (a) => { return a.x; })
      .attr('y1', (a) => { return a.ny + horzLineYOffset; })
      .attr('x2', (a) => { return a.x+ (a.note.label.length * this._annotationCharWidth); })
      .attr('y2', (a) => { return a.ny + horzLineYOffset; })
      .attr("stroke-width", 0.5)
      .attr("stroke", lineColor);   

  }


}
