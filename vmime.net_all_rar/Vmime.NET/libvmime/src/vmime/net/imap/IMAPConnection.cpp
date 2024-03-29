//
// VMime library (http://www.vmime.org)
// Copyright (C) 2002-2013 Vincent Richard <vincent@vmime.org>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 3 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// Linking this library statically or dynamically with other modules is making
// a combined work based on this library.  Thus, the terms and conditions of
// the GNU General Public License cover the whole combination.
//

#include "../vmime/config.hpp"


#if VMIME_HAVE_MESSAGING_FEATURES && VMIME_HAVE_MESSAGING_PROTO_IMAP


#include "../vmime/net/imap/IMAPTag.hpp"
#include "../vmime/net/imap/IMAPConnection.hpp"
#include "../vmime/net/imap/IMAPUtils.hpp"
#include "../vmime/net/imap/IMAPStore.hpp"

#include "../vmime/exception.hpp"
#include "../vmime/platform.hpp"

#include "../vmime/utility/stringUtils.hpp"

#include "../vmime/net/defaultConnectionInfos.hpp"

#if VMIME_HAVE_SASL_SUPPORT
	#include "../vmime/security/sasl/SASLContext.hpp"
#endif // VMIME_HAVE_SASL_SUPPORT

#if VMIME_HAVE_TLS_SUPPORT
	#include "../vmime/net/tls/TLSSession.hpp"
	#include "../vmime/net/tls/TLSSecuredConnectionInfos.hpp"
#endif // VMIME_HAVE_TLS_SUPPORT

#include <sstream>


// Helpers for service properties
#define GET_PROPERTY(type, prop) \
	(m_store.acquire()->getInfos().getPropertyValue <type>(getSession(), \
		dynamic_cast <const IMAPServiceInfos&>(m_store.acquire()->getInfos()).getProperties().prop))
#define HAS_PROPERTY(prop) \
	(m_store.acquire()->getInfos().hasProperty(getSession(), \
		dynamic_cast <const IMAPServiceInfos&>(m_store.acquire()->getInfos()).getProperties().prop))


namespace vmime {
namespace net {
namespace imap {

// FIX by Elmue: static instance counter for Trace output
int IMAPConnection::g_instanceID = 0;

IMAPConnection::IMAPConnection(ref <IMAPStore> store, ref <security::authenticator> auth)
	: m_store(store), m_auth(auth), m_socket(NULL), m_parser(NULL), m_tag(NULL),
	  m_hierarchySeparator('\0'), m_state(STATE_NONE), m_timeoutHandler(NULL),
	  m_secured(false), m_firstTag(true), m_capabilitiesFetched(false), m_noModSeq(false)
{
	// FIX by Elmue: increment static instance counter, copy to member variable
	m_instanceID = ++g_instanceID;
}


IMAPConnection::~IMAPConnection()
{
	try
	{
		if (isConnected())
			disconnect();
		else if (m_socket)
			internalDisconnect();
	}
	catch (vmime::exception&)
	{
		// Ignore
	}

	// FIX by Elmue: decrement static instance counter
	g_instanceID --;
}


void IMAPConnection::connect()
{
	if (isConnected())
		throw exceptions::already_connected();

	m_state = STATE_NONE;
	m_hierarchySeparator = '\0';

	const string address = GET_PROPERTY(string, PROPERTY_SERVER_ADDRESS);
	const port_t port = GET_PROPERTY(port_t, PROPERTY_SERVER_PORT);

	ref <IMAPStore> store = m_store.acquire();

	// Create the time-out handler
	if (store->getTimeoutHandlerFactory())
		m_timeoutHandler = store->getTimeoutHandlerFactory()->create();

	// Create and connect the socket
	m_socket = store->getSocketFactory()->create(m_timeoutHandler);

#if VMIME_HAVE_TLS_SUPPORT
	if (store->isIMAPS())  // dedicated port/IMAPS
	{
		ref <tls::TLSSession> tlsSession = tls::TLSSession::create
			(store->getCertificateVerifier(),
			 store->getSession()->getTLSProperties());

		ref <tls::TLSSocket> tlsSocket =
			tlsSession->getSocket(m_socket);

		m_socket = tlsSocket;

	    // FIX by Elmue: Added Console independent Trace
	    #if VMIME_TRACE
		    TRACE("IMAP Connecting to '%s' on port %d using SSL encryption. (%s)", address.c_str(), port, tlsSession->getLibraryVersion().c_str());
	    #endif

		m_secured = true;
		m_cntInfos = vmime::create <tls::TLSSecuredConnectionInfos>(address, port, tlsSession, tlsSocket);
	}
	else
#endif // VMIME_HAVE_TLS_SUPPORT
	{
	    // FIX by Elmue: Added Console independent Trace
	    #if VMIME_TRACE
		    TRACE("IMAP Connecting to '%s' on port %d. (not encrypted)", address.c_str(), port);
	    #endif

		m_cntInfos = vmime::create <defaultConnectionInfos>(address, port);
	}

	m_socket->connect(address, port);

	m_tag = vmime::create <IMAPTag>();
	// FIX by Elmue: Added instance counter for Trace output
	m_parser = vmime::create <IMAPParser>(m_tag, m_socket, m_timeoutHandler, m_instanceID);


	setState(STATE_NON_AUTHENTICATED);


	// Connection greeting
	//
	// eg:  C: <connection to server>
	// ---  S: * OK mydomain.org IMAP4rev1 v12.256 server ready

	utility::auto_ptr <IMAPParser::greeting> greet(m_parser->readGreeting());
	bool needAuth = false;

	if (greet->resp_cond_bye())
	{
		internalDisconnect();
		throw exceptions::connection_greeting_error(greet->getErrorLog());
	}
	else if (greet->resp_cond_auth()->condition() != IMAPParser::resp_cond_auth::PREAUTH)
	{
		needAuth = true;
	}

	if (greet->resp_cond_auth()->resp_text()->resp_text_code() &&
		greet->resp_cond_auth()->resp_text()->resp_text_code()->capability_data())
	{
		processCapabilityResponseData(greet->resp_cond_auth()->resp_text()->resp_text_code()->capability_data());
	}

#if VMIME_HAVE_TLS_SUPPORT
	// Setup secured connection, if requested
	const bool tls = HAS_PROPERTY(PROPERTY_CONNECTION_TLS)
		&& GET_PROPERTY(bool, PROPERTY_CONNECTION_TLS);
	const bool tlsRequired = HAS_PROPERTY(PROPERTY_CONNECTION_TLS_REQUIRED)
		&& GET_PROPERTY(bool, PROPERTY_CONNECTION_TLS_REQUIRED);

	if (!store->isIMAPS() && tls)  // only if not IMAPS
	{
		try
		{
			startTLS();
		}
		// Non-fatal error
		catch (exceptions::command_error&)
		{
			if (tlsRequired)
			{
				m_state = STATE_NONE;
				throw;
			}
			else
			{
				// TLS is not required, so don't bother
			}
		}
		// Fatal error
		catch (...)
		{
			m_state = STATE_NONE;
			throw;
		}
	}
#endif // VMIME_HAVE_TLS_SUPPORT

	// Authentication
	if (needAuth)
	{
		try
		{
			authenticate();
		}
		catch (...)
		{
			m_state = STATE_NONE;
			throw;
		}
	}

	// Get the hierarchy separator character
	initHierarchySeparator();

	// Switch to state "Authenticated"
	setState(STATE_AUTHENTICATED);
}


void IMAPConnection::authenticate()
{
	getAuthenticator()->setService(m_store.acquire());

	// FIX by Elmue: Show ALL error messages from the server
	string s_Error;

    #if VMIME_HAVE_SASL_SUPPORT
	    // First, try SASL authentication
	    if (GET_PROPERTY(bool, PROPERTY_OPTIONS_SASL))
	    {
		    try
		    {
			    authenticateSASL();
			    return;
		    }
		    catch (exceptions::authentication_error& e)
		    {
			    if (!GET_PROPERTY(bool, PROPERTY_OPTIONS_SASL_FALLBACK))
			    {
				    // Can't fallback on normal authentication
				    internalDisconnect();
				    throw e;
			    }
			    else // Ignore, will try normal authentication
			    {
				    // FIX by Elmue: Show ALL error messages from the server
				    s_Error = string(e.response()) + "\n";

				    if (!m_socket->isConnected())
				    {
					    internalDisconnect();
					    throw exceptions::authentication_error(s_Error + "The server has disconnected.");
				    }
			    }
		    }
		    catch (exception& e)
		    {
			    internalDisconnect();
			    throw e;
		    }
	    }
    #endif // VMIME_HAVE_SASL_SUPPORT

	// Normal authentication
	const string username = getAuthenticator()->getUsername();
	const string password = getAuthenticator()->getPassword();

	// FIX by Elmue: Don't print password to Trace output
	send(true, "LOGIN " + IMAPUtils::quoteString(username)
		+ " " + IMAPUtils::quoteString(password), true, "LOGIN {User} {Password}");

	utility::auto_ptr <IMAPParser::response> resp(m_parser->readResponse());

	if (resp->isBad())
	{
		internalDisconnect();
		throw exceptions::command_error("LOGIN", resp->getErrorLog());
	}

	// FIX by Elmue: Added
	const IMAPParser::resp_cond_state* state = resp->response_done()->response_tagged()->resp_cond_state();

	if (state->status() != IMAPParser::resp_cond_state::OK)
	{
		internalDisconnect();
		
		// FIX by Elmue: Showing ALL server errors
		throw exceptions::authentication_error(s_Error + "LOGIN: " + state->resp_text()->text());
	}

	// Server capabilities may change when logged in
	if (!processCapabilityResponseData(resp))
		invalidateCapabilities();
}


#if VMIME_HAVE_SASL_SUPPORT

void IMAPConnection::authenticateSASL()
{
	if (!getAuthenticator().dynamicCast <security::sasl::SASLAuthenticator>())
		throw exceptions::authentication_error("No SASL authenticator available.");

	const std::vector <string> capa = getCapabilities();
	std::vector <string> saslMechs;

	for (unsigned int i = 0 ; i < capa.size() ; ++i)
	{
		const string& x = capa[i];

		if (x.length() > 5 &&
			(x[0] == 'A' || x[0] == 'a') &&
			(x[1] == 'U' || x[1] == 'u') &&
			(x[2] == 'T' || x[2] == 't') &&
			(x[3] == 'H' || x[3] == 'h') &&
			x[4] == '=')
		{
			saslMechs.push_back(string(x.begin() + 5, x.end()));
		}
	}

	// FIX by Elmue: Added Trace of all implemented mechanisms
	security::sasl::SASLMechanismFactory::traceImplementedMechanisms();

	// FIX by Elmue: Error message improved
	if (saslMechs.empty())
		throw exceptions::authentication_error("The server does not support any SASL mechanism.");

	std::vector <ref <security::sasl::SASLMechanism> > mechList;

	ref <security::sasl::SASLContext> saslContext =
		vmime::create <security::sasl::SASLContext>();

	string s_Avail;
	for (unsigned int i = 0 ; i < saslMechs.size() ; ++i)
	{
		try
		{
			mechList.push_back
				(saslContext->createMechanism(saslMechs[i]));

			if (s_Avail.length())
				s_Avail += ", ";

			s_Avail += saslMechs[i];
		}
		catch (exceptions::no_such_mechanism&)
		{
			// Ignore mechanism
		}
	}

	// FIX by Elmue: Error message improved
	if (mechList.empty())
		throw exceptions::authentication_error("The server does not support any of the implemented SASL mechanisms.");

	// FIX by Elmue: Added Console independent Trace
	#if VMIME_TRACE
		TRACE("SASL (%d) Available mechanisms on this server: %s", m_instanceID, s_Avail.c_str());
	#endif

	// Try to suggest a mechanism among all those supported
	ref <security::sasl::SASLMechanism> suggestedMech =
		saslContext->suggestMechanism(mechList);

	if (!suggestedMech)
		throw exceptions::authentication_error("Unable to suggest SASL mechanism.");

	// Allow application to choose which mechanisms to use
	mechList = getAuthenticator().dynamicCast <security::sasl::SASLAuthenticator>()->
		getAcceptableMechanisms(mechList, suggestedMech);

	if (mechList.empty())
		throw exceptions::authentication_error("No SASL mechanism available.");

	// FIX by Elmue: Add server error message to exception:
	string s_Error;

	// Try each mechanism in the list in turn
	for (unsigned int i = 0 ; i < mechList.size() ; ++i)
	{
		ref <security::sasl::SASLMechanism> mech = mechList[i];

		ref <security::sasl::SASLSession> saslSession =
			saslContext->createSession("imap", getAuthenticator(), mech);

		saslSession->init();

		send(true, "AUTHENTICATE " + mech->getName(), true);

		for (bool cont = true ; cont ; )
		{
			utility::auto_ptr <IMAPParser::response> resp(m_parser->readResponse());

			if (resp->response_done() &&
				resp->response_done()->response_tagged() &&
				resp->response_done()->response_tagged()->resp_cond_state()->
					status() == IMAPParser::resp_cond_state::OK)
			{
				m_socket = saslSession->getSecuredSocket(m_socket);
				return;
			}
			else
			{
				std::vector <IMAPParser::continue_req_or_response_data*>
					respDataList = resp->continue_req_or_response_data();

				string response;
				bool hasResponse = false;

				for (unsigned int i = 0 ; i < respDataList.size() ; ++i)
				{
					if (respDataList[i]->continue_req())
					{
						response = respDataList[i]->continue_req()->resp_text()->text();
						hasResponse = true;
						break;
					}
				}

				if (!hasResponse)
				{
					// FIX by Elmue: Add server error message to exception
					s_Error += "\nSASL " + mech->getName() + ": " + resp->response_done()
										   ->response_tagged()->resp_cond_state()->resp_text()->text();

					cont = false;
					continue;
				}

				byte_t* challenge = 0;
				long challengeLen = 0;

				// FIX by Elmue: It is a bad idea to define another variable with the same name here!
				byte_t* respBuf = 0;
				long respLen = 0;

				try
				{
					// Extract challenge
					saslContext->decodeB64(response, &challenge, &challengeLen);

					// Prepare response
					saslSession->evaluateChallenge
						(challenge, challengeLen, &respBuf, &respLen);

					// Send response
					// FIX by Elmue: Don't show base64 encoded password in Trace
					send(false, saslContext->encodeB64(respBuf, respLen), true, "{Authentication Data}");

					// Server capabilities may change when logged in
					invalidateCapabilities();
				}
				catch (exceptions::sasl_exception& e)
				{
					// FIX by Elmue: Added exception message
					s_Error += string("\n") + e.what();

					if (challenge)
					{
						delete [] challenge;
						challenge = NULL;
					}

					if (resp)
					{
						delete [] respBuf;
						respBuf = NULL;
					}

					// Cancel SASL exchange
					send(false, "*", true);
				}
				catch (...)
				{
					if (challenge)
						delete [] challenge;

					if (resp)
						delete [] respBuf;

					throw;
				}

				if (challenge)
					delete [] challenge;

				if (resp)
					delete [] respBuf;
			}
		}
	}

	// FIX by Elmue:
	throw exceptions::authentication_error(s_Error);
}

#endif // VMIME_HAVE_SASL_SUPPORT


#if VMIME_HAVE_TLS_SUPPORT

void IMAPConnection::startTLS()
{
	try
	{
		send(true, "STARTTLS", true);

		utility::auto_ptr <IMAPParser::response> resp(m_parser->readResponse());

		if (resp->isBad() || resp->response_done()->response_tagged()->
			resp_cond_state()->status() != IMAPParser::resp_cond_state::OK)
		{
			throw exceptions::command_error
				("STARTTLS", resp->getErrorLog(), "bad response");
		}

		ref <tls::TLSSession> tlsSession = tls::TLSSession::create
			(m_store.acquire()->getCertificateVerifier(),
			 m_store.acquire()->getSession()->getTLSProperties());

		ref <tls::TLSSocket> tlsSocket =
			tlsSession->getSocket(m_socket);

		tlsSocket->handshake(m_timeoutHandler);

		// FIX by Elmue: Added Trace output
		#if VMIME_TRACE
			TRACE("IMAP Encrypted TLS session started. (%s)", tlsSession->getLibraryVersion().c_str());
		#endif

		m_socket = tlsSocket;
		m_parser->setSocket(m_socket);

		m_secured = true;
		m_cntInfos = vmime::create <tls::TLSSecuredConnectionInfos>
			(m_cntInfos->getHost(), m_cntInfos->getPort(), tlsSession, tlsSocket);

		// " Once TLS has been started, the client MUST discard cached
		//   information about server capabilities and SHOULD re-issue the
		//   CAPABILITY command.  This is necessary to protect against
		//   man-in-the-middle attacks which alter the capabilities list prior
		//   to STARTTLS. " (RFC-2595)
		invalidateCapabilities();
	}
	catch (exceptions::command_error&)
	{
		// Non-fatal error
		throw;
	}
	catch (exception&)
	{
		// Fatal error
		internalDisconnect();
		throw;
	}
}

#endif // VMIME_HAVE_TLS_SUPPORT


const std::vector <string> IMAPConnection::getCapabilities()
{
	if (!m_capabilitiesFetched)
		fetchCapabilities();

	return m_capabilities;
}


bool IMAPConnection::hasCapability(const string& capa)
{
	if (!m_capabilitiesFetched)
		fetchCapabilities();

	const string normCapa = utility::stringUtils::toUpper(capa);

	for (unsigned int i = 0, n = m_capabilities.size() ; i < n ; ++i)
	{
		if (m_capabilities[i] == normCapa)
			return true;
	}

	return false;
}


void IMAPConnection::invalidateCapabilities()
{
	m_capabilities.clear();
	m_capabilitiesFetched = false;
}


void IMAPConnection::fetchCapabilities()
{
	send(true, "CAPABILITY", true);

	utility::auto_ptr <IMAPParser::response> resp(m_parser->readResponse());

	if (resp->response_done()->response_tagged()->
		resp_cond_state()->status() == IMAPParser::resp_cond_state::OK)
	{
		processCapabilityResponseData(resp);
	}
}


bool IMAPConnection::processCapabilityResponseData(const IMAPParser::response* resp)
{
	const std::vector <IMAPParser::continue_req_or_response_data*>& respDataList =
		resp->continue_req_or_response_data();

	for (unsigned int i = 0 ; i < respDataList.size() ; ++i)
	{
		if (respDataList[i]->response_data() == NULL)
			continue;

		const IMAPParser::capability_data* capaData =
			respDataList[i]->response_data()->capability_data();

		if (capaData == NULL)
			continue;

		processCapabilityResponseData(capaData);
		return true;
	}

	return false;
}


void IMAPConnection::processCapabilityResponseData(const IMAPParser::capability_data* capaData)
{
	std::vector <string> res;

	std::vector <IMAPParser::capability*> caps = capaData->capabilities();

	for (unsigned int j = 0 ; j < caps.size() ; ++j)
	{
		if (caps[j]->auth_type())
			res.push_back("AUTH=" + caps[j]->auth_type()->name());
		else
			res.push_back(utility::stringUtils::toUpper(caps[j]->atom()->value()));
	}

	m_capabilities = res;
	m_capabilitiesFetched = true;
}


ref <security::authenticator> IMAPConnection::getAuthenticator()
{
	return m_auth;
}


bool IMAPConnection::isConnected() const
{
	return (m_socket && m_socket->isConnected() &&
			(m_state == STATE_AUTHENTICATED || m_state == STATE_SELECTED));
}


bool IMAPConnection::isSecuredConnection() const
{
	return m_secured;
}


ref <connectionInfos> IMAPConnection::getConnectionInfos() const
{
	return m_cntInfos;
}


void IMAPConnection::disconnect()
{
	// FIX by Elmue: removed exception here
    if (!isConnected())
        return;

	internalDisconnect();
}


void IMAPConnection::internalDisconnect()
{
	if (isConnected())
	{
		send(true, "LOGOUT", true);

		m_socket->disconnect();
		m_socket = NULL;
	}

	m_timeoutHandler = NULL;

	m_state = STATE_LOGOUT;

	m_secured = false;
	m_cntInfos = NULL;
}


void IMAPConnection::initHierarchySeparator()
{
	send(true, "LIST \"\" \"\"", true);

	vmime::utility::auto_ptr <IMAPParser::response> resp(m_parser->readResponse());

	if (resp->isBad() || resp->response_done()->response_tagged()->
		resp_cond_state()->status() != IMAPParser::resp_cond_state::OK)
	{
		internalDisconnect();
		throw exceptions::command_error("LIST", resp->getErrorLog(), "bad response");
	}

	const std::vector <IMAPParser::continue_req_or_response_data*>& respDataList =
		resp->continue_req_or_response_data();

	bool found = false;

	for (unsigned int i = 0 ; !found && i < respDataList.size() ; ++i)
	{
		if (respDataList[i]->response_data() == NULL)
			continue;

		const IMAPParser::mailbox_data* mailboxData =
			static_cast <const IMAPParser::response_data*>
				(respDataList[i]->response_data())->mailbox_data();

		if (mailboxData == NULL || mailboxData->type() != IMAPParser::mailbox_data::LIST)
			continue;

		if (mailboxData->mailbox_list()->quoted_char() != '\0')
		{
			m_hierarchySeparator = mailboxData->mailbox_list()->quoted_char();
			found = true;
		}
	}

	if (!found) // default
		m_hierarchySeparator = '/';
}

// FIX by Elmue: Added parameter s8_Trace (if != NULL -> print s8_Trace instead of the command sent to server)
void IMAPConnection::send(bool tag, const string& what, bool end, const char* s8_Trace) // =NULL
{
	if (tag && !m_firstTag)
		++(*m_tag);

	// FIX by Elmue: Added Trace output
	string command;
	if (tag)
	{
		command += string(*m_tag);
		command += " ";
	}

	command += what;

	#if VMIME_TRACE
		if (s8_Trace == NULL) // don't show command if it contains a password
			s8_Trace = command.c_str();

		TRACE("IMAP (%d) send > \"%s\"", m_instanceID, s8_Trace);
	#endif

	if (end)
		command += "\r\n";

	m_socket->send(command);

	if (tag)
		m_firstTag = false;
}


void IMAPConnection::sendRaw(const char* buffer, const int count)
{
	m_socket->sendRaw(buffer, count);
}


IMAPParser::response* IMAPConnection::readResponse(IMAPParser::literalHandler* lh)
{
	return (m_parser->readResponse(lh));
}


IMAPConnection::ProtocolStates IMAPConnection::state() const
{
	return (m_state);
}


void IMAPConnection::setState(const ProtocolStates state)
{
	m_state = state;
}


char IMAPConnection::hierarchySeparator() const
{
	return (m_hierarchySeparator);
}


ref <const IMAPStore> IMAPConnection::getStore() const
{
	return m_store.acquire();
}


ref <IMAPStore> IMAPConnection::getStore()
{
	return m_store.acquire();
}


ref <session> IMAPConnection::getSession()
{
	return m_store.acquire()->getSession();
}


ref <const socket> IMAPConnection::getSocket() const
{
	return m_socket;
}


bool IMAPConnection::isMODSEQDisabled() const
{
	return m_noModSeq;
}


void IMAPConnection::disableMODSEQ()
{
	m_noModSeq = true;
}


} // imap
} // net
} // vmime


#endif // VMIME_HAVE_MESSAGING_FEATURES && VMIME_HAVE_MESSAGING_PROTO_IMAP

