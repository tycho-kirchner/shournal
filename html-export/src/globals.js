

import GenericTextDialog from './generic_text_dialog';

export let humanDateFormat;
export let humanDateFormatOnlyDate;
export let humanDateFormatOnlyTime;

export let d3TimeParseIsoWithMil;

export let textDialog;

export let commandList;
export let sessionTimeline;

export function init(){
  humanDateFormat = d3.timeFormat("%Y-%m-%d %H:%M");
  humanDateFormatOnlyDate = d3.timeFormat("%Y-%m-%d");
  humanDateFormatOnlyTime = d3.timeFormat("%H:%M");

  d3TimeParseIsoWithMil = d3.timeParse("%Y-%m-%dT%H:%M:%S.%L");

  textDialog = new GenericTextDialog();
}
