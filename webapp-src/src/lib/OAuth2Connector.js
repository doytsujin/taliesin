import Cookies from 'js-cookie';
import $ from 'jquery';

class OAuth2Connector {
	
	constructor(parameters) {
		var self = this;
		// internal
		this.refreshToken = false;
		this.accessToken = false;
		this.changeStatusCb = [];
		this.connected = false;
		this.parameters = {};
		this.localStorageKey = "taliesinOauth2";
		this.refreshTimeout = false;
		
		if (parameters) {
			this.parameters.storageType = parameters.storageType || "none";
			this.parameters.responseType = parameters.responseType || "code";
			this.parameters.serverUrl = parameters.serverUrl || "";
			this.parameters.scope = parameters.scope || "";
			this.parameters.authUrl = parameters.authUrl || "";
			this.parameters.tokenUrl = parameters.tokenUrl || "";
			this.parameters.clientId = parameters.clientId || "";
			this.parameters.clientPassword = parameters.clientPassword || "";
			this.parameters.redirectUri = parameters.redirectUri || "";
			if (parameters.changeStatusCb) {
				this.changeStatusCb.push(parameters.changeStatusCb);
			}
		}
		
		var storedData;
		var token;
		if (this.parameters.responseType === "token") {
			token = this.getTokenFromFragment();
			if (token) {
				this.accessToken = token;
				document.location = "#";
				this.broadcastMessage("connected", token.access_token);
				this.storeAccessToken(token);
			} else {
				storedData = this.getStoredData();
				if (storedData && storedData.accessToken && this.isTokenValid(storedData.accessToken)) {
					this.accessToken = storedData.accessToken;
					this.broadcastMessage("connected", this.accessToken.access_token);
				} else {
					this.broadcastMessage("disconnected");
					this.accessToken = false;
				}
			}
		} else if (this.parameters.responseType === "code") {
			var code = this.getCodeFromQuery();
			if (code) {
				this.getRefreshTokenFromCode(code, function (refreshToken) {
					if (refreshToken) {
						self.refreshToken = refreshToken.refresh_token;
						self.storeRefreshToken(self.refreshToken);
						if (refreshToken.access_token) {
							self.accessToken = {access_token: refreshToken.access_token, iat: refreshToken.iat, expires_in: refreshToken.expires_in};
							self.storeAccessToken(self.accessToken);
							self.broadcastMessage("connected", self.accessToken.access_token);
							self.refreshTokenLoop(refreshToken.refresh_token, self.accessToken.expires_in);
						} else {
							self.broadcastMessage("disconnected");
						}
					} else {
						self.broadcastMessage("disconnected");
					}
				});
				window.history.pushState(null, "", document.location.href.split("?")[0]);
			} else {
				storedData = this.getStoredData();
				if (storedData && storedData.accessToken && this.isTokenValid(storedData.accessToken)) {
					this.accessToken = storedData.accessToken;
					this.broadcastMessage("connected", this.accessToken.access_token);
					if (storedData.refreshToken) {
						var curDate = new Date();
						var timeout = Math.floor(((self.accessToken.iat + self.accessToken.expires_in)*1000 - curDate.getTime())/1000);
						this.refreshTokenLoop(storedData.refreshToken, timeout);
					}
				} else if (storedData && storedData.refreshToken) {
					this.accessToken = false;
					this.refreshToken = storedData.refreshToken;
					this.getAccessTokenFromRefresh(storedData.refreshToken, function (accessToken) {
						if (accessToken) {
							self.accessToken = accessToken;
							self.storeAccessToken(accessToken);
							self.refreshTokenLoop(storedData.refreshToken, self.accessToken.expires_in);
							self.broadcastMessage("connected", accessToken.access_token);
						} else {
							self.broadcastMessage("disconnected");
						}
					});
				} else {
					this.broadcastMessage("disconnected");
				}
			}
		} else {
			this.broadcastMessage("error");
		}
	}
	
  getQueryParams(qs) {
    qs = qs.split('+').join(' ');

    var params = {},
      tokens,
      re = /[#&]?([^=]+)=([^&]*)/g;

		tokens = re.exec(qs);
    while (tokens) {
      params[decodeURIComponent(tokens[1])] = decodeURIComponent(tokens[2]);
			tokens = re.exec(qs);
    }

    return params;
  }
	
	getTokenFromFragment() {
		var params = this.getQueryParams(document.location.hash);
		var curDate = new Date();
		if (params.access_token && params.expires_in)  {
			return {access_token: params.access_token, expires_in: params.expires_in, iat: Math.floor(curDate.getTime()/1000)};
		} else {
			return false;
		}
	}
	
	getCodeFromQuery() {
		var params = this.getQueryParams(document.location.search.substring(1));
		return params.code||false;
	}
	
	refresh(cb) {
		var self = this;
		if (this.parameters.responseType === "code" && this.refreshToken) {
			this.getAccessTokenFromRefresh(this.refreshToken, function (accessToken) {
				if (accessToken) {
					self.accessToken = accessToken;
					self.storeAccessToken(accessToken);
					self.refreshTokenLoop(self.refreshToken, self.accessToken.expires_in);
					self.broadcastMessage("refresh", accessToken.access_token);
					cb(accessToken.access_token);
				} else {
					self.broadcastMessage("disconnected");
					cb(false);
				}
			});
		} else {
			cb(false);
		}
	}
	
	getToken() {
		if (this.accessToken && this.isTokenValid(this.accessToken)) {
			return this.accessToken.access_token;
		} else {
			this.accessToken = false;
			return false;
		}
	}
	
	getStatus() {
		if (this.accessToken && this.isTokenValid(this.accessToken)) {
			return "connected";
		} else {
			this.accessToken = false;
			return "disconnected";
		}
	}
	
	storeAccessToken(token) {
		var storedObject = this.getStoredData();
		if (!storedObject) {
			storedObject = {};
		}
		storedObject.accessToken = token;
		
		if (this.parameters.storageType === "local") {
			return localStorage.setItem(this.localStorageKey, JSON.stringify(storedObject));
		} else if (this.parameters.storageType === "cookie") {
			return Cookies.set(this.localStorageKey, JSON.stringify(storedObject));
		} else {
			return false;
		}
	}
	
	storeRefreshToken(token) {
		var storedObject = this.getStoredData();
		if (!storedObject) {
			storedObject = {};
		}
		storedObject.refreshToken = token;
		
		if (this.parameters.storageType === "local") {
			return localStorage.setItem(this.localStorageKey, JSON.stringify(storedObject));
		} else if (this.parameters.storageType === "cookie") {
			return Cookies.set(this.localStorageKey, JSON.stringify(storedObject));
		} else {
			return false;
		}
	}
	
	getStoredData() {
    var storage;
		if (this.parameters.storageType === "local") {
			storage = JSON.parse(localStorage.getItem(this.localStorageKey));
      if (storage) {
        return storage;
      } else {
        return {};
      }
		} else if (this.parameters.storageType === "cookie") {
			storage = JSON.parse(Cookies.get(this.localStorageKey));
      if (storage) {
        return storage;
      } else {
        return {};
      }
		} else {
			return {};
		}
	}
	
	onChangeStatus(cb) {
		this.changeStatusCb.push(cb);
	}
	
	isTokenValid(token) {
		if (!!token) {
			var curDate = new Date();
			return ((token.iat + token.expires_in)*1000) > curDate.getTime();
		} else {
			return false;
		}
	}
	
	getRefreshTokenFromCode(code, cb) {
		var self = this;
		$.ajax({
			type: "POST",
			url: this.parameters.serverUrl + "/" + this.parameters.tokenUrl,
			data: {grant_type: "authorization_code", client_id: this.parameters.clientId, redirect_uri: this.parameters.redirectUri, code: code},
			success: function (result, status, request) {
				cb(result);
			},
			error: function (error) {
				if (error.status === 403) {
					self.refreshToken = false;
				}
				self.accessToken = false;
				cb(false);
			}
		});
	}
	
	getAccessTokenFromRefresh(refreshToken, cb) {
		var self = this;
		$.ajax({
			type: "POST",
			url: this.parameters.serverUrl + "/" + this.parameters.tokenUrl,
			data: {grant_type: "refresh_token", refresh_token: refreshToken},
			success: function (result, status, request) {
				cb(result);
			},
			error: function (error) {
				if (error.status === 403) {
					self.refreshToken = false;
				}
				self.accessToken = false;
				cb(false);
			}
		});
	}
	
	refreshTokenLoop(refreshToken, timeout) {
		var self = this;
		clearTimeout(this.refreshTimeout);
		this.refreshTimeout = setTimeout(function () {
			$.ajax({
				type: "POST",
				url: self.parameters.serverUrl + "/" + self.parameters.tokenUrl,
				data: {grant_type: "refresh_token", refresh_token: refreshToken},
				success: function (result, status, request) {
					self.accessToken = result;
					self.storeAccessToken(result);
					self.broadcastMessage("refresh", result.access_token);
					self.refreshTokenLoop(refreshToken, self.accessToken.expires_in);
				},
				error: function (error) {
					if (error.status === 403) {
						self.refreshToken = false;
					}
					self.accessToken = false;
					self.broadcastMessage("disconnected");
				}
			});
		}, (timeout - 60)*1000);
	}
	
	broadcastMessage(status, token) {
		for (var i in this.changeStatusCb) {
			this.changeStatusCb[i](status, token);
		}
	}
	
	connect() {
		var token = this.getStoredData();
		if (token && this.isTokenValid(token.accessToken)) {
			this.broadcastMessage("connected", token.accessToken.access_token);
		} else {
			token.accessToken = false;
			this.storeAccessToken(false);
			if (this.parameters.responseType === "token") {
				document.location = this.parameters.serverUrl + "/" + this.parameters.authUrl + "?response_type=token&client_id=" + this.parameters.clientId + "&redirect_uri=" + this.parameters.redirectUri + "&scope=" + this.parameters.scope;
			} else if (this.parameters.responseType === "code") {
				document.location = this.parameters.serverUrl + "/" + this.parameters.authUrl + "?response_type=code&client_id=" + this.parameters.clientId + "&redirect_uri=" + this.parameters.redirectUri + "&scope=" + this.parameters.scope;
			}
		}
	}
  
  disconnect() {
    clearTimeout(this.refreshTimeout);
    this.refreshToken = false;
    this.accessToken = false;
    this.broadcastMessage("disconnected");
		if (this.parameters.storageType === "local") {
			localStorage.removeItem(this.localStorageKey);
		} else if (this.parameters.storageType === "cookie") {
			Cookies.remove(this.localStorageKey);
		}
  }
}

export default OAuth2Connector;
