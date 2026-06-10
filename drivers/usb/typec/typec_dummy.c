#include <linux/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/usb/typec.h>

struct typec_partner *typec_register_partner(struct typec_port *port,
                                             struct typec_partner_desc *desc)
{
    return NULL;
}
EXPORT_SYMBOL_GPL(typec_register_partner);

void typec_unregister_partner(struct typec_partner *partner)
{
}
EXPORT_SYMBOL_GPL(typec_unregister_partner);

struct typec_port *typec_register_port(struct device *parent,
                                       const struct typec_capability *cap)
{
    return NULL;
}
EXPORT_SYMBOL_GPL(typec_register_port);

void typec_unregister_port(struct typec_port *port)
{
}
EXPORT_SYMBOL_GPL(typec_unregister_port);

void typec_set_data_role(struct typec_port *port, enum typec_data_role role)
{
}
EXPORT_SYMBOL_GPL(typec_set_data_role);

void typec_set_pwr_role(struct typec_port *port, enum typec_role role)
{
}
EXPORT_SYMBOL_GPL(typec_set_pwr_role);

void typec_set_vconn_role(struct typec_port *port, enum typec_role role)
{
}
EXPORT_SYMBOL_GPL(typec_set_vconn_role);

void typec_set_pwr_opmode(struct typec_port *port,
                          enum typec_pwr_opmode opmode)
{
}
EXPORT_SYMBOL_GPL(typec_set_pwr_opmode);
