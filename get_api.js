fetch('https://data.fingrid.fi/api/notifications/active', {
    method: 'GET',
    // Request headers
    headers: {
        'Cache-Control': 'no-cache',
        'x-api-key': build.env.APIKEY,
    }
})
    .then(response => {
        console.log(response.status);
        console.log(response.text());
    })
    .catch(err => console.error(err));