
import * as globals from './globals';

/**
 * Parse the command-date into d3's date and assign session colors
 * @param {[Command]} commands
 */
export function prepareCommands(commands){
  commands.forEach(function(cmd) {
    cmd.startTime = globals.d3TimeParseIsoWithMil(cmd.startTime);
    cmd.endTime = globals.d3TimeParseIsoWithMil(cmd.endTime);
  });
  _fillCommandSessionColors(commands);
}

/**
 * Can be passed to array.sort or similar functions.
 * @param {*} cmd1 
 * @param {*} cmd2 
 * @return {int}
 */
export function compareStartDates(cmd1, cmd2) {
  return cmd1.startTime - cmd2.startTime;
}

/**
 * Can be passed to array.sort or similar functions.
 * @param {*} cmd1 
 * @param {*} cmd2 
 * @return {int}
 */
export function compareEndDates(cmd1, cmd2) {
  return cmd1.endTime - cmd2.endTime;
}


/**
 * Assign session-colors to the given commands.
 * Each session gets a specific color, after n sessions occurred, colors
 * start from beginning again.
 *  @param {[Command]} commands
*/
function _fillCommandSessionColors(commands){
  const DISTINCT_COLORS = [
    '#e6194b', '#3cb44b', '#ffe119', '#4363d8', '#f58231', '#911eb4', '#46f0f0',
    '#f032e6', '#bcf60c', '#fabebe', '#008080', '#e6beff', '#9a6324', '#fffac8',
    '#800000', '#aaffc3', '#808000', '#ffd8b1', '#000075', '#808080',
  ];
  let lastColorIdx = 0;
  const sessionColorMap = new Map();
  commands.forEach(function(cmd) {
    if(cmd.sessionUuid === null){
      cmd.sessionColor = '#000000';
    } else {
      let color = sessionColorMap.get(cmd.sessionUuid);
      if(color === undefined){
        color = DISTINCT_COLORS[lastColorIdx];
        sessionColorMap.set(cmd.sessionUuid, color);
        lastColorIdx++;
        if(lastColorIdx >= DISTINCT_COLORS.length){
          lastColorIdx = 0;
        }
      }
      cmd.sessionColor = color;
    }
  });
}
