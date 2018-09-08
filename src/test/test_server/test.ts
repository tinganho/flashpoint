import express from 'express';
import program from 'commander';
import bodyParser from 'body-parser';
import fs from 'fs';

const app = express();
app.set('etag', false);
app.use((_, res, next) => {
    res.removeHeader('X-Powered-By');
    res.removeHeader('ETag');
    res.removeHeader('Connection');
    next();
});

interface Headers {
    [name: string]: string;
}

interface Response {
    assertRequestMethod?: 'GET' | 'POST' | 'PATCH' | 'PUT' | 'HEAD';
    assertRequestPath?: string;
    assertGraphqlQuery?: string;
    assertGraphqlQueryLocation?: 'payload' | 'query-parameter';
    headers?: Headers;
    status?: number;
    body?: string;
}

program
  .version('0.1.0')
  .option('--responses <responses>', 'Responses')
  .option('--fd <fd>', 'File descriptor')
  .parse(process.argv);

console.log('[TEST_SERVER] - Request: ');
console.log(program.responses);
const responses = JSON.parse(program.responses) as Response[];
app.use(bodyParser.json());
app.all('*', (req, res): void => {
    console.log('[TEST_SERVER] - Recieved:')
    console.log(req.method, req.url);
    for (const header in req.headers) {
        console.log(header, req.headers[header]);
    }
    console.log(req.body);
    const response = responses.pop();
    if (!response) {
        console.error('Unexpected request.');
        res.status(500).send('Unexpected request.');
        return;
    }
    if (response.assertRequestMethod && response.assertRequestMethod !== req.method) {
        console.error(`Expected path '${response.assertRequestPath}'.`);
        res.status(500).send(`Expected method '${response.assertRequestMethod}'.`);
        return;
    }
    if (typeof response.assertRequestPath !== 'undefined' && response.assertRequestPath !== req.path) {
        console.error(`Expected path '${response.assertRequestPath}'.`);
        res.status(500);
        return;
    }
    if (response.assertGraphqlQuery) {
        let graphqlQuery: string;
        if (response.assertGraphqlQueryLocation && response.assertGraphqlQueryLocation === 'query-parameter') {
            if (!req.query.query) {
                console.error(`Expected GraphQL query to be in location '${response.assertGraphqlQuery}'.`);
                res.status(500);
                return;
            }
            graphqlQuery = JSON.parse(req.query.query);
        }
        else if (response.assertGraphqlQueryLocation && response.assertGraphqlQueryLocation === 'payload') {
            if (!req.body.query) {
                console.error(`Expected GraphQL query to be in location '${response.assertGraphqlQuery}'.`);
                res.status(500);
                return;
            }
            graphqlQuery = JSON.parse(req.body.query);
        }
        else {
            graphqlQuery = JSON.parse(req.body.query || req.query.query);
        }
        if (graphqlQuery !== response.assertGraphqlQuery) {
            console.error(`Expected GraphQL query '${response.assertGraphqlQuery}'.`);
            res.status(500);
            return;
        }
    }
    if (response.headers) {
        for (const name in response.headers) {
            res.append(name, response.headers[name]);
        }
    }
    res.status(response.status || 200).send(response.body || '');
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