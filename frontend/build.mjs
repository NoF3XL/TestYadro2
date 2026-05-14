import * as esbuild from 'esbuild';

await esbuild.build({
    entryPoints: ['src/main.ts'],
    bundle: true,
    outfile: '../backend/static/app.js',
    format: 'iife',
    target: 'es2020',
    sourcemap: true,
    minify: false,
});

console.log('Frontend built successfully -> backend/static/app.js');
