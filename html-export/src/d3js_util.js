

/**
 * Wrap long axis labels to mutliple lines by maximum width, splitting words by 
 * the given *delimeter-keeping* splitStr and auto-truncating long words.
 * Leading and trailing whitespaces of each line are trimmed.
 * @param {[String]} tickTexts 
 * @param {int} width The max. width in pixels for each label 
 * @param {RegExp} splitStr A regular expression for the split-string, 
 * which keeps the delimeter, e.g. /(?=\s)/.
 */
export function wrapTextLabels(tickTexts, width, splitStr=/(?=\s)/) {
  tickTexts.each(function() {
    const text = d3.select(this);
    const words = text.text().split(splitStr);
    let line = [];
    let lineNumber = 0;
    const lineHeight = 1.1; // ems
    const y = text.attr("y");
    const dy = parseFloat(text.attr("dy"));
    let tspan = text.text(null).append("tspan").attr("x", 0).attr("y", y)
      .attr("dy", dy + "em");
    // only increment i, if the word can be for sure drawn to current line
    for(let i=0; i < words.length; ) {
      line.push(words[i]);
      tspan.text(line.join(''));
      if (tspan.node().getComputedTextLength() <= width) {
        ++i;
        continue;
      }

      if (line.length === 1) {
        // this single word is too long to fit into a line -> clip it
        _truncateAndSetLabelTxt(line[0].trim(), tspan, width);
        ++i;    
      } else {
        // this word does not fit any more -> put it to next line 
        // and render all others
        line.pop();
        tspan.text(line.join('').trim());
        
        // NO ++i -> the current word must be rendered in next line
      } 
      tspan = text.append("tspan").attr("x", 0).attr("y", y)
        .attr("dy", `${++lineNumber * lineHeight + dy}em`).text(null);
      line = [];
    }
  });
}


function _truncateAndSetLabelTxt(labelTxt, tspan, width) {
  do {
    labelTxt = labelTxt.slice(0, -3);
    tspan.text(labelTxt);
  } while (tspan.node().getComputedTextLength() > width);
  labelTxt = labelTxt.slice(0, -2);
  labelTxt += '..';
  tspan.text(labelTxt);
}
