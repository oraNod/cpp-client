/*
 * SecurityConfigurationBuilder.h
 *
 *  Created on: Jan 27, 2017
 *      Author: rigazilla
 */

#ifndef INCLUDE_INFINISPAN_HOTROD_SECURITYCONFIGURATIONBUILDER_H_
#define INCLUDE_INFINISPAN_HOTROD_SECURITYCONFIGURATIONBUILDER_H_
#include "infinispan/hotrod/ConfigurationBuilder.h"
#include "infinispan/hotrod/AuthenticationConfigurationBuilder.h"
#include "infinispan/hotrod/SslConfigurationBuilder.h"
namespace infinispan {
namespace hotrod {

/**
 * SecurityConfigurationBuilder contains all the authentication and TLS related settings.
 */
class SecurityConfigurationBuilder : public ConfigurationChildBuilder
{
public:
    SecurityConfigurationBuilder(ConfigurationBuilder& _builder) : ConfigurationChildBuilder(_builder), sslConfigurationBuilder(_builder) {}
    SecurityConfiguration create() {
       return SecurityConfiguration(sslConfigurationBuilder.create(), authenticationConfigurationBuilder.create());
    }
    /**
     * \return the SSL (TLS) configuration builder related to this
     */
    SslConfigurationBuilder& getSslConfigurationBuilder() { return sslConfigurationBuilder; }
    /**
     * \return the authentication configuration builder related to this
     */
    AuthenticationConfigurationBuilder& authentication() { return authenticationConfigurationBuilder; }
private:
    AuthenticationConfigurationBuilder authenticationConfigurationBuilder;
    SslConfigurationBuilder sslConfigurationBuilder;
};

}}



#endif /* INCLUDE_INFINISPAN_HOTROD_SECURITYCONFIGURATIONBUILDER_H_ */
