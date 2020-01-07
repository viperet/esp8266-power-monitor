const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const CopyPlugin = require('copy-webpack-plugin');
const path = require('path');
module.exports = {
    mode: 'development',
    entry: path.join(__dirname, 'src', 'index'),
    // watch: true,
    plugins: [
        new CopyPlugin([
            { from: path.join(__dirname, 'src', 'index.html'), to: './' },
        ]),
        new MiniCssExtractPlugin({
            filename: 'style.css',
        })
    ],
    output: {
        path: path.join(__dirname, 'data'),
        publicPath: '/',
        filename: "bundle.js",
        chunkFilename: '[name].js'
    },
    module: {
        rules: [{
            test: /.jsx?$/,
            include: [
                path.resolve(__dirname, 'src')
            ],
            exclude: [
                path.resolve(__dirname, 'node_modules')
            ],
            loader: 'babel-loader',
            query: {
                presets: [
                    ["@babel/env", {
                        "targets": {
                            "browsers": "last 2 chrome versions"
                        }
                    }]
                ]
            }
        },
        {
            test: /\.(scss|css)$/i,
            use: [
                MiniCssExtractPlugin.loader,
                {
                    loader: 'css-loader',
                    options: {
                        sourceMap: true,
                    },
                },
                {
                    loader: 'sass-loader',
                    options: {
                        sourceMap: true,
                    },
                },
            ],
        },
        {
            test: /\.(woff(2)?|ttf|eot|svg)(\?v=\d+\.\d+\.\d+)?$/,
            use: [{
                loader: 'file-loader',
                options: {
                    name: '[name].[ext]',
                    outputPath: './'
                }
            }],
        }]
    },
    resolve: {
        extensions: ['.json', '.js', '.jsx']
    },
    // devtool: 'source-map',
    devServer: {
        contentBase: path.join(__dirname, '/data/'),
        inline: true,
        host: 'localhost',
        port: 8080,
    }
};