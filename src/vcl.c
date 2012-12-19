#define _GNU_SOURCE
#include "main.h"
#include "plugins.h"
#include "ipc.h"
#include "httpd.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

struct vcl_priv_t {
	int logger;
	int vadmin;
};

unsigned int vcl_reply(struct httpd_request *request, void *data)
{
	struct agent_core_t *core = data;
	struct httpd_response response;
	struct vcl_priv_t *vcl;
	struct agent_plugin_t *plug;
	struct ipc_ret_t vret;
	char *cmd;
	int ret;
	int id;

	plug = plugin_find(core,"vcl");
	vcl = plug->data;

	response.status = 200;
	vret.answer = "Invalid VCL-request";
	if (request->method == M_GET) {
		if (!strcmp(request->url, "/vcl") || !strcmp(request->url,"/vcl/")) {
			ipc_run(vcl->vadmin, "vcl.list", &vret);
		} else if (!strncmp(request->url,"/vcl/",strlen("/vcl/"))) { 
			ret = asprintf(&cmd,"vcl.show %s",request->url + strlen("/vcl/"));
			assert(ret>0);
			ipc_run(vcl->vadmin, cmd, &vret);
			free(cmd);
		}
	} else if (request->method == M_POST) {
		id = time(NULL);
		logger(vcl->logger, "Storing VCL");
		ret = asprintf(&cmd, "vcl.inline %d << __EOF_%d__\n%s\n__EOF_%d__",id,id,(char *)request->data,id);
		assert(ret>0);
		ipc_run(vcl->vadmin, cmd, &vret);
		free(cmd);
	} else {
		vret.answer = "Not a GET request";
	}
	response.body = vret.answer;
	response.nbody = strlen(vret.answer);
	send_response(request->connection, &response);
	return 0;
}

void
vcl_init(struct agent_core_t *core)
{
	struct agent_plugin_t *plug;
	struct vcl_priv_t *priv = malloc(sizeof(struct vcl_priv_t));
	plug = plugin_find(core,"vcl");
	
	priv->logger = ipc_register(core,"logd");
	priv->vadmin = ipc_register(core,"vadmin");
	plug->data = (void *)priv;
	plug->start = NULL;
        httpd_register_url(core, "/vcl", M_GET, vcl_reply, core);
}


