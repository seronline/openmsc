/**
 * @author Sebastian Robitzsch <srobitzsch@gmail.com>
 *
 * @section LICENSE
 *
 * OpenMSC - MSCgen-Based Control-Plane Traffic Emulator
 *
 * Copyright (C) 2014 Sebastian Robitzsch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/gamma_distribution.hpp>
#include "eventIdGenerator.hh"

void EventIdGenerator::Init(ReadMsc *rMsc_)
{
	readMsc_ = rMsc_;
}

void EventIdGenerator::InitLog(log4cxx::LoggerPtr l)
{
	logger = l;
}

USE_CASE_ID EventIdGenerator::DetermineUseCaseId()
{
	USE_CASE_ID useCaseId;
	int numOfUseCases;
	timespec seed;
	numOfUseCases = (*readMsc_).GetNumOfUseCases();
//TODO read probabilities from openmsc.msc file and determine useCaseId based on this
	useCaseId = 1;

	return useCaseId;
}

EVENT_ID_VECTOR EventIdGenerator::GetEventIdForComDescr(USE_CASE_ID useCaseId,
		int step,
		BS_ID bsId,
		UE_ID ueId)
{
	EVENT_ID_VECTOR eIdV;
	IDENTIFIER sourceId,
	destinationId,
	protocolTypeId,
	primitiveNameId,
	informationElementId,
	informationElementValueId;
	COMMUNICATION_DESCRIPTION_STRUCT comDescrStruct;
	// obtaining the communication description for this step
	comDescrStruct = (*readMsc_).GetParticularCommunicationDescription(useCaseId, step);
	sourceId = (*readMsc_).TranslateNetworkElement2ID(comDescrStruct.source, bsId, ueId);
	destinationId = (*readMsc_).TranslateNetworkElement2ID(comDescrStruct.destination, bsId, ueId);
	protocolTypeId = (*readMsc_).TranslateProtocolType2ID(comDescrStruct.protocolType);
	primitiveNameId = (*readMsc_).TranslatePrimitiveName2ID(comDescrStruct.primitiveName);
	// iterating over all information elements of this particular communication description
	for (unsigned int i = 0; i < comDescrStruct.informationElements.size(); i++)
	{
		ostringstream convert;
		informationElementId = (*readMsc_).TranslateInformationElement2ID(comDescrStruct.informationElements.at(i));
		// check if IE == UE_ID
//TODO proper parsing of IE values from openmsc.msc and openmsc.cfg
		if (comDescrStruct.informationElements.at(i) == "UE_ID")
			informationElementValueId = ueId;
		else if (comDescrStruct.informationElements.at(i) == "BS_ID")
			informationElementValueId = bsId;
		else
			informationElementValueId = 999; // dummy value (see to-do above)
		// Generating the EventID
		convert << setfill('0') << setw(5) << sourceId
				<< setw(5) << destinationId
				<< setw(2) << protocolTypeId
				<< setw(2) << primitiveNameId
				<< setw(2) << informationElementId
				<< setw(3) << informationElementValueId;
		eIdV.insert(eIdV.end(),convert.str());
	}

	return eIdV;
}
TIME EventIdGenerator::CalculateLatency(USE_CASE_ID ucId, int step, base_generator_type *gen)
{
	COMMUNICATION_DESCRIPTION_STRUCT comDescrStruct;
	TIME latency;
	comDescrStruct = (*readMsc_).GetParticularCommunicationDescription(ucId, step);
	if (comDescrStruct.latencyDescription.distribution == CONSTANT)
		latency = TIME(comDescrStruct.latencyDescription.constantLatency.millisec(), "millisec");
	else if (comDescrStruct.latencyDescription.distribution == EXPONENTIAL)
	{
		boost::exponential_distribution<> exp_dist (comDescrStruct.latencyDescription.exponentialLambda);
		boost::variate_generator<base_generator_type&, boost::exponential_distribution<> > exponential (*gen, exp_dist);
		latency = TIME(exponential(), "millisec");
	}
	else if (comDescrStruct.latencyDescription.distribution == GAUSSIAN)
	{
		boost::normal_distribution<> gaussian_dist (comDescrStruct.latencyDescription.gaussianMu, comDescrStruct.latencyDescription.gaussianSigma);
		boost::variate_generator<base_generator_type&, boost::normal_distribution<> > gaussian (*gen, gaussian_dist);
		latency = TIME(gaussian(), "millisec");
		while (latency.sec() > (comDescrStruct.latencyDescription.gaussianMu + 10*comDescrStruct.latencyDescription.gaussianSigma))
			latency = TIME(gaussian(), "millisec");
	}
	else
	{
		latency = TIME(0.0, "millisec");
		LOG4CXX_ERROR (logger, "Latency distribution " << comDescrStruct.latencyDescription.distribution << " has not been implemented for use case " << ucId << ", Step " << step);
	}

	if (latency.sec() > 10000)
		LOG4CXX_TRACE (logger, "Problem with boost::*_distribution<> Please file a bug report on https://code.google.com/p/openmsc/issues/list");

	LOG4CXX_TRACE (logger, "Latency for use-case ID " << ucId
			<< ", Step " << step
			<< " = " << latency.millisec() <<
			"ms\t(Distribution = " << comDescrStruct.latencyDescription.distribution << ")");
	return TIME(latency.millisec(), "millisec");
}





