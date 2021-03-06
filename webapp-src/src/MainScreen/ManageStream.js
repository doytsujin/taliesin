import React, { Component } from 'react';
import { Table, DropdownButton, Button, ButtonGroup, MenuItem } from 'react-bootstrap';
import FontAwesome from 'react-fontawesome';

import StateStore from '../lib/StateStore';
import ModalConfirm from '../Modal/ModalConfirm';
import ModalEdit from '../Modal/ModalEdit';
import i18n from '../lib/i18n';

class ManageStream extends Component {	
	constructor(props) {
		super(props);
		
		this.state = {
			streamList: StateStore.getState().streamList, 
			modalConfirmShow: false, 
			modalRenameShow: false, 
			modalSaveShow: false, 
			modalTitle: "", 
			modalMessage: "", 
			modalValue: "",
			curStream: false
		};

		StateStore.subscribe(() => {
			var reduxState = StateStore.getState();
			if ((reduxState.lastAction === "setStreamList" || reduxState.lastAction === "setStream") && this._ismounted) {
				this.setState({streamList: reduxState.streamList}, () => {
					this.buildStreamExternalList();
				});
			}
		});
		
		this.playStream = this.playStream.bind(this);
		this.playStreamExternal = this.playStreamExternal.bind(this);
		this.deleteStream = this.deleteStream.bind(this);
		this.renameStream = this.renameStream.bind(this);
		this.saveStream = this.saveStream.bind(this);
		this.detailsStream = this.detailsStream.bind(this);
		this.reloadStream = this.reloadStream.bind(this);
		this.resetStream = this.resetStream.bind(this);
		this.confirmDelete = this.confirmDelete.bind(this);
		this.confirmRename = this.confirmRename.bind(this);
		this.confirmSave = this.confirmSave.bind(this);
		this.reloadStreamList = this.reloadStreamList.bind(this);
		this.buildStreamExternalList = this.buildStreamExternalList.bind(this);
		this.buildStreamExternal = this.buildStreamExternal.bind(this);
	}
	
	componentWillReceiveProps(nextProps) {
		this.setState({
			streamList: StateStore.getState().streamList, 
			modalConfirmShow: false, 
			modalRenameShow: false, 
			modalSaveShow: false, 
			modalTitle: "", 
			modalMessage: "", 
			modalValue: "",
			curStream: false
		});
	}
	
	componentDidMount() {
		this._ismounted = true;
		this.buildStreamExternalList();
	}

	componentWillUnmount() {
		this._ismounted = false;
	}
	
	buildStreamExternalList() {
		var streamList = this.state.streamList;
		streamList.forEach((stream) => {
			stream.external = this.buildStreamExternal(stream);
		});
		this.setState({streamList: streamList});
	}
	
	buildStreamExternal(stream) {
		if (stream) {
			if (stream.webradio) {
				return "data:application/mpegurl;base64," + btoa("#EXTM3U\n\n#EXTINF:0," + (stream.display_name||"no name") + "\n" + StateStore.getState().taliesinApiUrl + "/stream/" + stream.name + "\n");
			} else {
				return StateStore.getState().taliesinApiUrl + "/stream/" + stream.name + "?url_prefix=" + StateStore.getState().taliesinApiUrl;
			}
		} else {
			return "";
		}
	}

	playStream(stream) {
		StateStore.dispatch({type: "loadStreamAndPlay", stream: stream, index: 0});
	}
	
	deleteStream(stream) {
		this.setState({modalConfirmShow: true, modalTitle: i18n.t("stream.remove_title"), modalMessage: i18n.t("stream.remove_message", {stream: (stream.display_name||i18n.t("common.no_name"))}), curStream: stream});
	}
	
	renameStream(stream) {
		this.setState({modalRenameShow: true, modalTitle: i18n.t("stream.rename_title"), modalMessage: i18n.t("stream.rename_message", {stream: (stream.display_name||i18n.t("common.no_name"))}), modalValue: stream.display_name, curStream: stream});
	}
	
	saveStream(stream) {
		this.setState({modalSaveShow: true, modalTitle: i18n.t("stream.save_as_playlist_title"), modalMessage: i18n.t("stream.save_as_playlist_message", {stream: (stream.display_name||i18n.t("common.no_name"))}), modalValue: stream.display_name, curStream: stream});
	}
	
	detailsStream(stream) {
		StateStore.dispatch({type: "setStreamDetails", stream: stream});
		StateStore.dispatch({type: "setCurrentBrowse", browse: "streamDetails"});
	}
	
	reloadStream(stream) {
		StateStore.getState().APIManager.taliesinApiRequest("PUT", "/stream/" + encodeURIComponent(stream.name) + "/manage", {command: "reload"})
		.then(() => {
			StateStore.getState().NotificationManager.addNotification({
				message: i18n.t("stream.message_stream_reload_ok"),
				level: 'info'
			});
		})
		.fail(() => {
			StateStore.getState().NotificationManager.addNotification({
				message: i18n.t("stream.message_stream_reload_error"),
				level: 'error'
			});
		});
	}
	
	resetStream(stream) {
		StateStore.getState().APIManager.taliesinApiRequest("PUT", "/stream/" + encodeURIComponent(stream.name) + "/manage", {command: "reset_url"})
		.then((result) => {
			var streamList = StateStore.getState().streamList;
			for (var i in streamList) {
				if (streamList[i].name === stream.name) {
					streamList[i].name = result.name;
					break;
				}
			}
			StateStore.dispatch({type: "setStreamList", streamList: streamList});
			this.setState({streamList: streamList});
			StateStore.getState().NotificationManager.addNotification({
				message: i18n.t("stream.message_stream_reset_ok"),
				level: 'info'
			});
		})
		.fail(() => {
			StateStore.getState().NotificationManager.addNotification({
				message: i18n.t("stream.message_stream_reset_error"),
				level: 'error'
			});
		});
	}
	
	confirmDelete(confirm) {
		if (confirm) {
			StateStore.getState().APIManager.taliesinApiRequest("PUT", "/stream/" + encodeURIComponent(this.state.curStream.name) + "/manage", {command: "stop"})
			.then(() => {
				var streamList = StateStore.getState().streamList;
				for (var i in streamList) {
					if (streamList[i].name === this.state.curStream.name) {
						streamList.splice(i, 1);
						break;
					}
				}
				StateStore.dispatch({type: "setStreamList", streamList: streamList});
				if (StateStore.getState().profile.stream.name === this.state.curStream.name) {
					StateStore.dispatch({type: "loadStream", stream: false});
				}
				this.setState({streamList: streamList, modalConfirmShow: false});
				StateStore.getState().NotificationManager.addNotification({
					message: i18n.t("stream.message_stream_delete_ok"),
					level: 'info'
				});
			})
			.fail(() => {
				StateStore.getState().NotificationManager.addNotification({
					message: i18n.t("stream.message_stream_delete_error"),
					level: 'error'
				});
				this.setState({modalConfirmShow: false});
			});
		} else {
			this.setState({modalConfirmShow: false});
		}
	}
	
	confirmRename(name) {
		if (name) {
			StateStore.getState().APIManager.taliesinApiRequest("PUT", "/stream/" + encodeURIComponent(this.state.curStream.name) + "/manage", {command: "rename", parameters: {name: name}})
			.then(() => {
				var streamList = StateStore.getState().streamList;
				for (var i in streamList) {
					if (streamList[i].name === this.state.curStream.name) {
						streamList[i].display_name = name;
						break;
					}
				}
				StateStore.dispatch({type: "setStreamList", streamList: streamList});
				StateStore.getState().NotificationManager.addNotification({
					message: i18n.t("stream.message_stream_rename_ok"),
					level: 'info'
				});
				this.setState({streamList: streamList, modalRenameShow: false});
			})
			.fail(() => {
				StateStore.getState().NotificationManager.addNotification({
					message: i18n.t("stream.message_stream_rename_error"),
					level: 'error'
				});
				this.setState({modalRenameShow: false});
			});
		} else {
			this.setState({modalRenameShow: false});
		}
	}
	
	confirmSave(name) {
		if (name) {
			StateStore.getState().APIManager.taliesinApiRequest("PUT", "/stream/" + encodeURIComponent(this.state.curStream.name) + "/manage", {command: "save", parameters: {name: name}})
			.then(() => {
				StateStore.getState().NotificationManager.addNotification({
					message: i18n.t("stream.message_stream_save_ok"),
					level: 'info'
				});
				this.setState({modalSaveShow: false});
			})
			.fail(() => {
				StateStore.getState().NotificationManager.addNotification({
					message: i18n.t("stream.message_stream_save_error"),
					level: 'error'
				});
				this.setState({modalSaveShow: false});
			});
		} else {
			this.setState({modalSaveShow: false});
		}
	}
	
	reloadStreamList() {
		StateStore.getState().APIManager.taliesinApiRequest("GET", "/stream")
		.then((result) => {
			StateStore.dispatch({type: "setStreamList", streamList: result});
			this.setState({streamList: result});
			StateStore.getState().NotificationManager.addNotification({
				message: i18n.t("stream.message_stream_list_reload_ok"),
				level: 'info'
			});
		})
		.fail((result) => {
			StateStore.getState().NotificationManager.addNotification({
				message: i18n.t("stream.message_stream_list_reload_error"),
				level: 'error'
			});
		});
	}
	
	playStreamExternal(stream) {
		$("#play-external-anchor-"+stream.name)[0].click();
	}
	
	render() {
		var rows = [];
		this.state.streamList.forEach((stream, index) => {
			var type, random;
			if (stream.webradio) {
				type = i18n.t("common.webradio");
				if (stream.random) {
					random = <FontAwesome name={"random"} />;
				}
			} else {
				type = i18n.t("common.jukebox");
			}
			rows.push(
				<tr key={index}>
					<td>
						<a role="button" onClick={() => this.detailsStream(stream)}>
							{stream.display_name||i18n.t("common.no_name")}
						</a>
					</td>
					<td>
						<a role="button" onClick={() => this.detailsStream(stream)}>
							{type} {random}
						</a>
					</td>
					<td className="hidden-xs">
						<a role="button" onClick={() => this.detailsStream(stream)}>
							{stream.elements}
						</a>
					</td>
					<td className="hidden-xs">
						<a role="button" onClick={() => this.detailsStream(stream)}>
							{stream.format + " - " + (stream.stereo?i18n.t("common.stereo"):i18n.t("common.mono")) + " - " + stream.sample_rate + " " + i18n.t("common.khz") + " - " + (stream.bitrate/1000) + " " + i18n.t("common.bps")}
						</a>
					</td>
					<td>
						<a role="button" onClick={() => this.detailsStream(stream)}>
							{(stream.clients && stream.clients.length)||0}
						</a>
					</td>
					<td className="text-center">
						<a href={stream.external} style={{display: "none"}} id={"play-external-anchor-" + stream.name} download={(stream.display_name||i18n.t("common.no_name"))+".m3u"}>{i18n.t("common.external")}</a>
						<ButtonGroup className="hidden-xs hidden-sm">
							<Button title={i18n.t("common.play_now")} onClick={() => this.playStream(stream)}>
								<FontAwesome name={"play"} />
							</Button>
							<Button title={i18n.t("common.external")} onClick={() => this.playStreamExternal(stream)}>
								<FontAwesome name={"external-link"} />
							</Button>
							<Button title={i18n.t("common.rename")} onClick={() => this.renameStream(stream)}>
								<FontAwesome name={"pencil"} />
							</Button>
							<Button title={i18n.t("stream.save_as_playlist")} onClick={() => this.saveStream(stream)}>
								<FontAwesome name={"floppy-o"} />
							</Button>
							<Button title={i18n.t("stream.reload")} onClick={() => this.reloadStream(stream)}>
								<FontAwesome name={"exchange"} />
							</Button>
							<Button title={i18n.t("stream.reset_url")} onClick={() => this.resetStream(stream)}>
								<FontAwesome name={"unlock-alt"} />
							</Button>
							<Button title={i18n.t("stream.delete")} onClick={() => this.deleteStream(stream)}>
								<FontAwesome name={"trash"} />
							</Button>
						</ButtonGroup>
						<DropdownButton className="visible-xs visible-sm" id={"xs-manage-"+stream.name} pullRight title={
							<span><i className="fa fa-cog"></i></span>
						}>
							<MenuItem onClick={() => this.playStream(stream)}>
								<FontAwesome name={"play"} className="space-after"/>
								{i18n.t("common.play_now")}
							</MenuItem>
							<MenuItem onClick={() => this.playStreamExternal(stream)}>
								<FontAwesome name={"external-link"} className="space-after"/>
								{i18n.t("common.external")}
							</MenuItem>
							<MenuItem onClick={() => this.renameStream(stream)}>
								<FontAwesome name={"pencil"} className="space-after"/>
								{i18n.t("common.rename")}
							</MenuItem>
							<MenuItem onClick={() => this.saveStream(stream)}>
								<FontAwesome name={"floppy-o"} className="space-after"/>
								{i18n.t("stream.save_as_playlist")}
							</MenuItem>
							<MenuItem onClick={() => this.reloadStream(stream)}>
								<FontAwesome name={"exchange"} className="space-after"/>
								{i18n.t("stream.reload")}
							</MenuItem>
							<MenuItem onClick={() => this.resetStream(stream)}>
								<FontAwesome name={"unlock-alt"} className="space-after"/>
								{i18n.t("stream.reset_url")}
							</MenuItem>
							<MenuItem onClick={() => this.deleteStream(stream)}>
								<FontAwesome name={"trash"} className="space-after"/>
								{i18n.t("stream.delete")}
							</MenuItem>
						</DropdownButton>
					</td>
				</tr>
			);
		});
		return (
			<div>
				<Button title={i18n.t("stream.reload")} onClick={this.reloadStreamList}>
					<FontAwesome name={"refresh"} />
				</Button>
				<Table striped bordered condensed hover>
					<thead>
						<tr>
							<th>{i18n.t("common.name")}</th>
							<th>{i18n.t("common.type")}</th>
							<th className="hidden-xs">{i18n.t("common.elements")}</th>
							<th className="hidden-xs">{i18n.t("stream.format")}</th>
							<th>{i18n.t("stream.clients")}</th>
							<th></th>
						</tr>
					</thead>
					<tbody>
						{rows}
					</tbody>
				</Table>
				<ModalConfirm show={this.state.modalConfirmShow} title={this.state.modalTitle} message={this.state.modalMessage} onCloseCb={this.confirmDelete} />
				<ModalEdit show={this.state.modalRenameShow} title={this.state.modalTitle} message={this.state.modalMessage} onCloseCb={this.confirmRename} value={this.state.modalValue} />
				<ModalEdit show={this.state.modalSaveShow} title={this.state.modalTitle} message={this.state.modalMessage} onCloseCb={this.confirmSave} value={this.state.modalValue} />
			</div>
		);
	}
}

export default ManageStream;
