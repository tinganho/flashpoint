"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const apollo_server_express_1 = require("apollo-server-express");
const morgan = require("morgan");
const express = require("express");
const bodyParser = require("body-parser");
const typeDefs = apollo_server_express_1.gql `
    type Query {
        "A simple type"
        hello: String
    }
`;
const resolvers = {
    Query: {
        hello: () => 'world',
    }
};
const server = new apollo_server_express_1.ApolloServer({
    typeDefs,
    resolvers
});
const app = express();
app.use(morgan('dev'));
app.use(bodyParser());
server.applyMiddleware({ app });
app.listen({ port: 4000 }, () => console.log(`Server ready at http://localhost:4000${server.graphqlPath}`));
// app.get('/graphql', async (req, res) => {
//     const result = await graphql(schema, req.query.query);
//     res.send(result.data);
// });
// app.listen(3000, () => {
//     console.log('Listening on http://localhost:3000.');
// });
//# sourceMappingURL=server.js.map