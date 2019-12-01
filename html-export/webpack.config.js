const LicenseWebpackPlugin = require('license-webpack-plugin').LicenseWebpackPlugin;

module.exports = {
  optimization:{
      minimize: false, // maybe_todo: set to true for release
  },
  plugins: [
    new LicenseWebpackPlugin()
  ],
  mode:'production',
}
