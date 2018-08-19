import { ApolloServer, gql } from 'apollo-server-express';
import morgan = require('morgan');
import express = require('express');
import bodyParser = require('body-parser');

const typeDefs = gql`
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

const server = new ApolloServer({
    typeDefs,
    resolvers
});

const app = express();
app.use(morgan('dev'));
app.use(bodyParser());
server.applyMiddleware({ app });
app.listen({ port: 4000 }, () =>
    console.log(`Server ready at http://localhost:4000${server.graphqlPath}`),
);

// app.get('/graphql', async (req, res) => {
//     const result = await graphql(schema, req.query.query);
//     res.send(result.data);
// });

// app.listen(3000, () => {
//     console.log('Listening on http://localhost:3000.');
// });
