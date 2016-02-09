/**
 * This file is part of OpenMediaVault.
 *
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    Volker Theile <volker.theile@openmediavault.org>
 *                          <fatogre500@gmail.com>
 * @copyright Copyright (c) 2009-2015 Volker Theile
 *
 * OpenMediaVault is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * OpenMediaVault is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenMediaVault. If not, see <http://www.gnu.org/licenses/>.
 */
// require("js/omv/window/Upload.js")
// require("js/omv/window/Window.js")
// require("js/omv/form/Panel.js")
Ext.define("OMV.module.admin.service.ldapserver.tools.Upload", {
   extend: "OMV.window.Upload",
   
   initComponent: function() {
      var me = this;
      Ext.apply(me, {
         buttons: [{
            text: _("Next Step"),
            handler: me.onOkButton,
            scope: me
         },{
            text: _("Cancel"),
            handler: me.onCancelButton,
            scope: me
         }],
         items: [ me.fp = Ext.create("OMV.form.Panel", {
            bodyPadding: "5 5 0",
            items: [{
               xtype: "displayfield",
               fieldLabel: _("<font color='red'>WARNING!</font>"),
               value: "This tool will remove all current LDAP Users and Groups and restore LDAP directory/settings from the selected backup!<br>" +
                "<font color='red'>There will be a confirmation window after this!</font>"
            },{
               xtype: "filefield",
               name: "file",
               fieldLabel: _("Backup LDAP File"),
               emptyText: "Select a LDIF File...",
               allowBlank: false
            }]
         }) ]
      });
      //skip calling OMV.window.Upload's initComponent method me.callSuperclass did not seem to work
      me.superclass.superclass.initComponent.call(me);
   },
      
   onUploadSuccess: function(form, action) {
      var me = this;
      // !!! Attention !!! Fire event before window is closed,
      // otherwise the dialog's own listener is removed before the
      // event has been fired and the action has been executed.
      me.fireEvent("success", me, action.result);
      // Now close the dialog.
      me.close();
      // Open a confirmation screen
      Ext.create("OMV.module.admin.service.ldapserver.tools.Restore", {
         listeners: {
            scope: me,
         }
      }).show();
   },
   
});

Ext.define("OMV.module.admin.service.ldapserver.tools.Restore", {
   extend: "OMV.workspace.window.Form",
   
   rpcService: "LDAPServer",
   rpcGetMethod: "getRestoreSettings",
   rpcSetMethod: "restoreLdapServer",
   
   title: _("Restore LDAP Server"),
   width: 500,
	hideResetButton: true,
   okButtonText: _("Confirm Restore"),
   
   getFormItems: function() {
      return [{
         xtype: "displayfield",
         fieldLabel: _("<font color='red'>WARNING!</font>"),
         value: "This tool will remove all current LDAP Users and Groups and restore LDAP directory/settings from the selected backup!<br>" +
                "<font color='red'>This action is irreversible afer confirmation!</font>"
      },{
         xtype: "passwordfield",
         name: "newbindpw",
         fieldLabel: _("Bind Password"),
         allowBlank: false,
         plugins: [{
            ptype: "fieldinfo",
            text: _("Please supply a new root bind password for the LDAP server. "+
                    "You may use the previous password if you remember it.")
         }]
      },{
         xtype: "textfield",
         name: "confirmrestore",
         fieldLabel: _("Confirmation"),
         allowBlank: false,
         plugins: [{
            ptype: "fieldinfo",
            text: _("Type RESTORE here to confirm Restore.")
         }]
      }];
   },
   
   isDirty: function() {
      return true;
   },
   
   isValid: function() {
		var me = this;
		if (!me.callParent(arguments))
			return false;
		var valid = true;
		var values = me.getValues();
		// Check the restore keyword.
		if (values.confirmrestore !== "RESTORE") {
			var msg = _("Confirmation Keyword incorrect!");
			me.markInvalid([
				{ id: "confirmrestore", msg: msg }
			]);
			valid = false;
		}
		return valid;
	}
   
});

Ext.define("OMV.module.admin.service.ldapserver.tools.Clear", {
   extend: "OMV.workspace.window.Form",
   
   rpcService: "LDAPServer",
   rpcGetMethod: "getClearSettings",
   rpcSetMethod: "clearLdapServer",
   
   title: _("Restore Default LDAP Server"),
   width: 500,
	hideResetButton: true,
   okButtonText: _("Confirm Delete"),
   
   getFormItems: function() {
      return [{
         xtype: "displayfield",
         fieldLabel: _("<font color='red'>WARNING!</font>"),
         value: "This will remove all current LDAP Users and Groups and restore the default LDAP directory and settings!<br>" +
                "<font color='red'>This action is irreversible afer confirmation!</font>"
      },{
         xtype: "textfield",
         name: "confirmdelete",
         fieldLabel: _("Confirmation"),
         allowBlank: false,
         plugins: [{
            ptype: "fieldinfo",
            text: _("Type DELETE here to confirm Deletion.")
         }]
      }];
   },
   
   isDirty: function() {
      return true;
   },
   
   isValid: function() {
		var me = this;
		if (!me.callParent(arguments))
			return false;
		var valid = true;
		var field = me.findField("confirmdelete");
		// Check the restore keyword.
		if (field.getValue() !== "DELETE") {
			var msg = _("Confirmation Keyword incorrect!");
			me.markInvalid([
				{ id: "confirmdelete", msg: msg }
			]);
			valid = false;
		}
		return valid;
	}
});
 
Ext.define("OMV.module.admin.service.ldapserver.Tools", {
   extend: "OMV.workspace.form.Panel",
   
   requires: [
      "OMV.window.Upload",
   ],
   
   rpcService: "LDAPServer",
   rpcGetMethod: "getToolSettings",
   rpcSetMethod: "setToolSettings",
   rpcBackupMethod: "getLdapContent",
   
   hideTopToolbar: true, 
   
   getFormItems: function() {
      var me = this;
      return [{
         xtype: "fieldset",
			title: _("Backup Local LDAP Server"),
			fieldDefaults: {
				labelSeparator: ""
			},
			items: [{
				xtype: "fieldcontainer",
            fieldLabel: _("Backup"),
				layout: "anchor",
				fieldDefaults: {
					anchor: "100%",
					labelSeparator: ""
				},
            items: [{
               xtype: "displayfield",
               value: "Create a backup file of all your Local LDAP server Users and Groups."
            },{
               xtype: "button",
               name: "backupbutton",
               text: _("Backup LDAP"),
               handler: Ext.Function.bind(me.onBackupButton,me,[ me ]),
               scope: me,
            }]
			}]
      },{
         xtype: "fieldset",
         title: _("Restore Local LDAP Server"),
         fieldDefaults: {
            labelSeparator: ""
         },
         items: [{
            xtype: "fieldcontainer",
            fieldLabel: _("Restore"),
            layout: "anchor",
            fieldDefaults: {
               anchor: "100%",
               labelSeparator: ""
            },
            items: [{
               xtype: "displayfield",
               value: "Restore your Users and Groups from a [backup] LDIF file.  (Warning: Doing this will erase your existing LDAP Users and Groups)"               
            },{
               xtype: "button",
               name: "restorebutton",
               text: _("Restore LDAP"),
               handler: Ext.Function.bind(me.onRestoreButton, me, [ me ]),
               scope: me,
            }]
         }]
      },{
         xtype: "fieldset",
         title: _("Clear Local LDAP Server"),
         fieldDefaults: {
            labelSeparator: ""
         },
         items: [{
            xtype: "fieldcontainer",
            fieldLabel: _("Clear/Default"),
            layout: "anchor",
            fieldDefaults: {
               anchor: "100%",
               labelSeparator: ""
            },
            items: [{
               xtype: "displayfield",
               value: "Clear the existing LDAP server and load the default LDAP server. (Warning: Doing this will erase your existing LDAP Users and Groups)"+
                      "<br>This results in an empty but usuable LDAP server.  Useful for when there's an unresolvable error with your LDAP server."
            },{
               xtype: "button",
               name: "clearbutton",
               text: _("Clear LDAP"),
               handler: function() {
                  var me = this;
                  Ext.create("OMV.module.admin.service.ldapserver.tools.Clear", {
                     listeners: {
                        scope: me,
                     }
                  }).show();
               }
            }]
         }]
      }]
   },
   
   onRestoreButton: function() {
      var me = this;
      Ext.create("OMV.module.admin.service.ldapserver.tools.Upload", {
         service: "LDAPServer",
         method: "uploadBackupFile",
         title: _("Restore LDAP from backup"),
         listeners: {
            scope: me,
            success: function(wnd, response) {
               // The upload was successful, now resynchronize the
               // package index files from their sources.                                        
            }
         }
      }).show();
   },
   
   onBackupButton: function() {
      var me = this;
      OMV.Download.request(me.rpcService,me.rpcBackupMethod, { id: "ldapbackup" });
   }
});
// Register a panel into the GUI.
//
// path: 
//     We want to add the panel in our example node. 
//     The node was configured with the path /service and the id example.
//     The path is therefore /service/example.
//
// className: 
//     The panel which should be registered and added (refers to 
//     the class name).
OMV.WorkspaceManager.registerPanel({
    id: "tools", 
    path: "/service/ldapserver", 
    text: _("LDAP Tools"),
    position: 15,
    className: "OMV.module.admin.service.ldapserver.Tools" 
});