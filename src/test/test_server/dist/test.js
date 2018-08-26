"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = __importDefault(require("express"));
const commander_1 = __importDefault(require("commander"));
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
    .option('--response <response>', 'Add response')
    .option('--fd <fd>', 'File descriptor')
    .option('--headers <headers>', 'Add headers')
    .option('--status [status]', 'Add status code', 200)
    .parse(process.argv);
app.all('/graphql', (_, res) => {
    if (commander_1.default.headers) {
        const headers = JSON.parse(commander_1.default.headers);
        for (const name in headers) {
            res.append(name, headers[name]);
        }
    }
    res.status(commander_1.default.status).send(commander_1.default.response);
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