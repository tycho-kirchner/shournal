
import * as command_manipulation from './command_manipulation';
import CommandList from './command_list';
import {assert} from './util';
import * as globals from './globals';
import SessionTimeline from './session_timeline';
import * as stats from './stats';

function displayErrorAtTop(msg){
  // vanilla js, since loading of libraries might have failed
  const errEl = document.getElementById('topError');
  errEl.style["visibility"] = "visible";
  errEl.innerHTML = msg;
}


function main() {
  if (scriptLoadError) {
    console.log(scriptLoadError);
    displayErrorAtTop(scriptLoadError);
    return;
  }

  globals.init();

  assert(commands.length > 0, 'commands.length > 0');

  const queryDate = globals.d3TimeParseIsoWithMil(ORIGINAL_QUERY_DATE_STR);
  const body = d3.select('body');

  body.append('button')
    .attr('class', 'btn btn-primary')
    .style('position', 'absolute')
    .style('right', '0px')
    .style('top', '0px')
    .html("Report Metadata")
    .on("click", () => {
      globals.textDialog.show("Report Metadata", 
        `Commandline-query (executed on ` + 
        `${globals.humanDateFormat(queryDate)}): ${ORIGINAL_QUERY}`);
    });

  command_manipulation.prepareCommands(commands);

  {
    let lastStart = commands[0].startTime;
    for(let i=1; i < commands.length; i++){
      assert(commands[i].startTime >= lastStart);
      lastStart = commands[i].startTime;
    }
  }
  

  const cmdFinalEndDate = globals.d3TimeParseIsoWithMil(CMD_FINAL_ENDDATE_STR);
 
  // Do not change order -> commandList.size computed based on sessionTimeLine.size.
  globals.sessionTimeline = new SessionTimeline(commands, cmdFinalEndDate);
  globals.commandList = new CommandList(commands);
  
  d3.select('#initialSpinner').remove();
  $(document).ready(stats.generateMiscStats);
}


try {
  main();
} catch (error) {
  console.log(error);
  displayErrorAtTop(error);
}
