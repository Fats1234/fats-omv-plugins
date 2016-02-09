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
Ext.define("OMV.module.admin.service.ldapserver.Settings", {
   extend: "OMV.workspace.form.Panel",
    
   // This path tells which RPC module and methods this panel will call to get 
   // and fetch its form values.
   rpcService: "LDAPServer",
   rpcGetMethod: "getSettings",
   rpcSetMethod: "setSettings",
    
   // getFormItems is a method which is automatically called in the 
   // instantiation of the panel. This method returns all fields for 
   // the panel.
   getFormItems: function() {
      return [{
         // xtype defines the type of this entry. Some different types
         // is: fieldset, checkbox, textfield and numberfield. 
         xtype: "fieldset",
         title: _("LDAP Server Settings"),
         fieldDefaults: {
            labelSeparator: ""
         },
         // The items array contains items inside the fieldset xtype.
         items: [{
            xtype: "checkbox",
            // The name option is sent together with is value to RPC
            // and is also used when fetching from the RPC.
            name: "enable",
            fieldLabel: _("Enable"),
            // checked sets the default value of a checkbox.
            checked: false,
            boxLabel: _("Enable/Disable The Local LDAP Server")
         },{
            xtype: "textfield",
            name: "basedn",
            fieldLabel: _("Base DN"),
            allowBlank: false,
            plugins: [{
               ptype: "fieldinfo",
               text: _("The Base DN that you would like to use.  Example: dc=mynet,dc=local")
            }]
         },{
            xtype: "textfield",
            name: "rootbindcn",
            fieldLabel: _("Root Bind CN"),
            allowBlank: false,
            plugins: [{
               ptype: "fieldinfo",
               text: _("The Root Bind Common Name that you would lke to use (Example: cn=admin).  Your Root Bind Distinguished Name (DN) will be this field concatenated with the base DN (Example: cn=admin,dc=mynet,dc=local).")
            }]
         },{
            xtype: "passwordfield",
            name: "rootbindpw",
            fieldLabel: _("Root Bind Password"),
            allowBlank: false,
            plugins: [{
               ptype: "fieldinfo",
               text: _("The Root Bind Password that you would like to use.")
            }]
         },{
            xtype: "textfield",
            name: "usersuffix",
            fieldLabel: _("Users Suffix"),
            allowBlank: true,
            plugins: [{
               ptype: "fieldinfo",
               text: _("Users Organizational Unit to use.  Example: ou=Users")
            }]
         },{
            xtype: "textfield",
            name: "groupsuffix",
            fieldLabel: _("Groups Suffix"),
            allowBlank: false,
            plugins: [{
               ptype: "fieldinfo",
               text: _("Groups Organizational Unit to use.  Example: ou=Groups")
            }]
         }]
      }];
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
    id: "settings", 
    path: "/service/ldapserver", 
    text: _("Settings"),
    position: 10,
    className: "OMV.module.admin.service.ldapserver.Settings" 
});