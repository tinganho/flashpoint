"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = __importDefault(require("express"));
const commander_1 = __importDefault(require("commander"));
const body_parser_1 = __importDefault(require("body-parser"));
const fs_1 = __importDefault(require("fs"));
const app = express_1.default();
app.set('etag', false);
app.use((_, res, next) => {
    res.removeHeader('X-Powered-By');
    res.removeHeader('ETag');
    res.removeHeader('Connection');
    next();
});
commander_1.default
    .version('0.1.0')
    .option('--responses <responses>', 'Responses')
    .option('--fd <fd>', 'File descriptor')
    .parse(process.argv);
console.log('[TEST_SERVER] - Request: ');
console.log(commander_1.default.responses);
const responses = JSON.parse(commander_1.default.responses);
app.use(body_parser_1.default.json());
app.all('*', (req, res) => {
    console.log('[TEST_SERVER] - Recieved:');
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
        let graphqlQuery;
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
        for (const name in response.headers) {
            res.append(name, response.headers[name]);
        }
    }
    res.status(response.status || 200).send(response.body || '');
});
const buffer = new Buffer(1024);
const server = app.listen(4000, () => {
    console.log('Listening on http://localhost:4000/graphql');
});
fs_1.default.read(parseInt(commander_1.default.fd, 10), buffer, 0, buffer.length, null, (error, bytesRead) => {
    if (error) {
        console.log(error);
        return;
    }
    if (bytesRead === 0) {
        server.close();
    }
});
//# sourceMappingURL=test.js.map