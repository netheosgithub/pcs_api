OAuth2
======

### Details regarding Oauth2 and repositories

Most providers use the OAuth2 protocol for authorizing your application to access files.
See online tutorials for OAuth2 workflows description.
pcs_api supports only web application workflow, so that authorization is persistent (thanks to refresh tokens).

- At first an application must be registered provider side.
- Copy id and secret given by provider (and also scope) to populate your AppInfoRepository
- then OAuth tokens must be fetched from provider OAuth server, to retrieve refresh tokens.
This implies a user manual agreement. A bootstrap utility is provided to help this step (see hereafter)

For quick tests, two repositories implementations are provided: `AppInfoFileRepository` and `UserCredentialsFileRepository`.
**These are rough and not suitable for production**. They store secrets in plain text files and do not scale to more
than 10 users (moreover, they are not multi-process safe).

Application developer is expected to provide better implementations (for example relying on a database with credentials ciphering).

#### AppInfoFileRepository:

This class expects a text file with "key=value" lines format.

*key* is composed of lower case provider name + dot + application name: `dropbox.mytestapp`

*value* is a json object containing the following keys and values:

  - appId: the provider client ID
  - appSecret: the provider client secret
  - scope: list of permissions (optional ; see provider docs for details).
  - redirectUrl: your web application URL (optional ; http://localhost may be used
    during development)

Example for a hubiC application with name 'mytestapp' (text is wrapped for display; should appear on a single line):

```
hubic.mytestapp = { "appId": "xxxx", "appSecret": "yyyy",
                    "scope":["usage.r","account.r","getAllLinks.r","credentials.r","activate.w","links.drw"],
                    "redirectUrl": "http://localhost/" }
```

Example for a Dropbox application: with Dropbox the code is displayed into web page, instead of being in redirect URL.
Dropbox also does not have any scope in OAuth2 exchanges. Scope is required however by pcs_api.
For full access, "dropbox" permission must be specified as scope ; for single folder, "sandbox" permission must be specified
(remember that scope is a list of permissions, hence the json array).

```
dropbox.mydropboxapp = { "appId": "xxxx", "appSecret": "yyyy", "scope": ["sandbox"] }
```

#### UserCredentialsFileRepository

This class stores users credentials in plain text in a `key=value` file.

*key* is composed of provider name + dot + application name + dot + user id: `dropbox.mytestapp.user@example.com`

*value* is a json object also, defining OAuth2 tokens, etc. Such tokens can be considered opaque, as they are returned
by providers and thus not constructed by a human or application.

However, for login/password providers (e.g. not OAuth2), the json object must contain a single attribute
`password` with value the user password. Example of value: `{"password":"xxxx"}`.


User credentials repository is populated by the bootstrap utility, and updated during pcs_api usage, when a token is refreshed.

#### Bootstrapping OAuth2: refresh_tokens

The boostrapping is asking user authorization (only web application workflow is supported for now).
Once user has given its authorization, tokens are persisted into users credentials repository.

In python, a sample script is available in the file [get_oauth_tokens.py](../samples/python/get_oauth_tokens.py).

A similar code sample is available for Java: [Main.java](../samples/java/src/main/java/net/netheos/pcsapi/sample/Main.java).
Basically, the following code is used:

```java
IStorageProvider provider = StorageFacade.forProvider(providerName)
                    .setAppInfoRepository(mAppRepo, mAppInfo.getAppName())
                    .setUserCredentialsRepository(mCredentialsRepo, null)
                    .setForBootstrapping(true) // To enable the bootstrapping
                    .build();

// Start bootstrapping
OAuth2Bootstrapper bootstrapper = new OAuth2Bootstrapper(provider);
// Get the URL to open in a browser
URI authorizeWebUrl = bootstrapper.getAuthorizeBrowserUrl();
[...
 Open (or ask user to open) browser with given url,
 Optional user login,
 User consents application access to his/her files,
 Browser is redirected, or a code is displayed into web page,
 User copy/paste url or code into application,
 url or code is available in variable 'redirectUrlOrCode'
 ...]
// Process the redirected url (with parameters)
bootstrapper.getUserCredentials(redirectUrlOrCode);
```

Basically user is asked to perform a manual authorization on the URL given by the tool.
After authorization, user must copy back full redirect url (or only the code displayed in web page) into python (or Java) tool
in order to finish the code workflow (code is exchanged against access_token, and also refresh_token).
Access token is then used to get user_id, and lastly user credentials are saved into repository.
From now on, authorization is granted to pcs_api for quite a long time (provider dependent).

Note for Google Drive (this should not occur anymore): sometimes google does not give any refresh token.
For tests purposes, it is recommended to go to google's user security page and revoke authorization for the application.

As a proof of concept, the sample application for Android shows how this can be performed in an application, by embedding a webview.
The [OAuth2AuthorizationActivity](../android/pcs-api-android/src/main/java/net/netheos/pcsapi/OAuth2AuthorizationActivity.java)
is started for inquiring user authorization.

Refer to the [Build page](build.md) for details on samples.
