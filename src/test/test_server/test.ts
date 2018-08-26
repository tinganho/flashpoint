import express from 'express';
import program from 'commander';
import fs from 'fs';

const app = express();
app.set('etag', false);
app.use((_, res, next) => {
    res.removeHeader('X-Powered-By');
    res.removeHeader('ETag');
    res.removeHeader('Connection');
    next();
});

program
  .version('0.1.0')
  .option('--response <response>', 'Add response')
  .option('--fd <fd>', 'File descriptor')
  .option('--headers <headers>', 'Add headers')
  .option('--status [status]', 'Add status code', 200)
  .parse(process.argv);

app.all('/graphql', (_, res) => {
    if (program.headers) {
        const headers = JSON.parse(program.headers);
        for (const name in headers) {
            res.append(name, headers[name]);
        }
    }
    res.status(program.status).send(program.response);
});
const buffer = new Buffer(1024);
const server = app.listen(4000, () => {
    console.log('Listening on http://localhost:4000/graphql');
});
fs.read(parseInt(program.fd, 10), buffer, 0, buffer.length, null, (error, bytesRead) => {
    if (error) {
        console.log(error);
        return;
    }
    if (bytesRead === 0) {
        server.close();
    }
});