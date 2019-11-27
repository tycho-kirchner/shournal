
import PlotMostWrittenFiles from './plot_most_written_files';
import PlotCmdCountPerCwd from './plot_cmdcount_per_cwd';
import PlotIoPerDir from './plot_io_per_dir';
import PlotCmdCountPerSession from './plot_cmdcount_per_session';
import { timedForEach } from './util';


export async function generateMiscStats() {
  const body = d3.select('body');

  if (typeof commands[0].fileWriteEvents === 'undefined') {
    // when generating from shournal, command-data (like fileWriteEvents)
    // is loaded later for performance reasons
    await timedForEach(commands, (cmd, idx) => {
      const cmdDataTag = d3.select('#commandDataJSON' + idx);
      const cmdData = JSON.parse(cmdDataTag.html());
      Object.assign(cmd, cmdData);
      cmdDataTag.remove();
    });
  }

  if (mostFileMods.length === 0 && sessionsMostCmds.length === 0 && 
      cwdCmdCounts.length === 0 && dirIoCounts.length === 0) {
    // No stats to display...
    return;
  }

  body.append('h3')
    .html('Miscellaneous statistics')
    .style('padding-top', '1em');

  const miscStatElement = body.append('div')
    .style('padding-top', '20px')
    .style('display', 'inline-block');

  if (mostFileMods.length > 0) {
    const plotMostWrittenFiles = new PlotMostWrittenFiles();
    plotMostWrittenFiles.generatePlot(commands, miscStatElement);
  }  
  
  if (sessionsMostCmds.length > 0) {
    const plotCmdCountPerSession = new PlotCmdCountPerSession();
    plotCmdCountPerSession.generatePlot(commands, miscStatElement);
  }

  if(cwdCmdCounts.length > 0){
    const plotCmdCountPerCwd = new PlotCmdCountPerCwd();
    plotCmdCountPerCwd.generatePlot(commands, miscStatElement);
  }
 
  if (dirIoCounts.length > 0) {
    const plotIoPerDir = new PlotIoPerDir();
    plotIoPerDir.generatePlot(commands, miscStatElement);
  }

  $('[data-toggle="tooltip"]').tooltip({
    delay: { show: 300, hide: 0 },
  });
}
