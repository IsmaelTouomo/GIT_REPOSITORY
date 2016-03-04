/*
 * CJsonPersistence.cpp
 *
 *  Created on: 28.12.2015
 *      Author: Youri
 */

#include "CJsonPersistence.h"
#include "CJsonScanner.h"
#include "CJsonToken.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>

#include <sstream>
#include <stdlib.h>     /* atof */
#include <algorithm> /* std::count*/

using namespace std;

#define WP_NUMBER_OF_ATTRIBUTS 3
#define POI_NUMBER_OF_ATTRIBUTS 5

CJsonPersistence::CJsonPersistence() {

}

CJsonPersistence::CJsonPersistence(std::string name) {
	this->m_mediaName = name;

}


void CJsonPersistence::setMediaName(std::string name) {
	this->m_mediaName = name;
}

bool CJsonPersistence::writeData(const CWpDatabase& waypointDb,
		const CPoiDatabase& poiDb) {

	int size = waypointDb.getSize(); // get the size of the waypoint database
	if(0 == size)
	{
		cout<<"the waypoint database is empty. No data to copy"<<endl;
		return false;
	}

	std::ofstream ofs;
	ofs.open(this->m_mediaName.c_str(), std::ofstream::out | std::ofstream::trunc );
	if(true != ofs.is_open())
	{
		cout<<"Could not open the file. Operation aborted"<<endl;
		return false;
	}

	CWaypoint** wpArray = new CWaypoint*[size];
	waypointDb.copyDatabase(wpArray); // copy the pointers to the waypoints in wpArray

	ofs << "{";
	ofs << std::endl;
	ofs << "	\"waypoints\": [ ";
	ofs << std::endl;
	for(int i = 0; i < size; i++)
	{
		ofs << "	{" << endl;
		ofs << "		\"name\": " << "\"" << wpArray[i]->getName() << "\"" << "," << endl;
	    ofs << "		\"latitude\": " << wpArray[i]->getLatitude() <<  "," <<endl;
	    ofs << "		\"longitude\": " << wpArray[i]->getLongitude()<<endl;
		ofs << "	},"<<endl;
		ofs.flush();
	}
	ofs << "	]," << endl;
	ofs << "  \"pois\": [" << endl;
	ofs.flush();

	size = poiDb.getSize(); // get the size of the POI database
	if(0 == size )
	{
		cout<<"The POI database is empty. Nothing to copy"<<endl;
		return false;
	}

	CPOI** poiArray = new CPOI*[size];
	poiDb.copyDatabase(poiArray); // copy the pointers in poiArray

	for(int i = 0; i < size; i++)
	{
		ofs << "	{" << endl;
		ofs << "		\"name\": " << "\"" << poiArray[i]->getName() << "\"" << "," << endl;
	    ofs << "		\"latitude\": " << poiArray[i]->getLatitude() <<  "," <<endl;
	    ofs << "		\"longitude\": " << poiArray[i]->getLongitude()<< "," <<endl;
	    ofs << "		\"type\": " << "\"" << type2String( poiArray[i]->getType()) << "\"" << "," <<endl;
	    ofs << "		\"description\": " << "\"" << poiArray[i]->getDescription() << "\"" <<endl;
		ofs << "	},"<<endl;
		ofs.flush();
	}
	ofs << "	]," << endl;
	ofs << "}" << endl;
	ofs.flush();
	return true;

}

bool CJsonPersistence::readData(CWpDatabase& waypointDb, CPoiDatabase& poiDb,
		MergeMode mode) {


	typedef enum {WaitingForFirstToken,
		          WaitingForDbName,
		          WaitingForDbBegin,
		          WaitingForAttributeName,
		          WaitingForNameSeparator,
		          WaitingForValueSeparator,
		          WaitingForValue,
		          WaitingForArrayBegin,
				  IsInErrorState} STATE;

	STATE actual_state =  WaitingForFirstToken;
	bool WPDB_read = false;
	bool POIDB_read = false;
	bool endArraySymbolRead = false;
	bool attributeNameRead = false;
	CPOI poi;
	string lastReadAttributeName;
	ifstream ifs(this->m_mediaName.c_str());
	int storeOperationCounter = 0;

	if(true != ifs.is_open())
	{
		cout<<"The Media could not be opened."<<" Operation failed!"<<endl;
		return false;
	}

	if( REPLACE == mode)
	{
		waypointDb.clearWpDatabase();
		poiDb.clearDatabase();
	}
	APT::CJsonScanner jsonScanner(ifs);
	APT::CJsonStringToken* jsonToken;
	while(1)
	{
		try
		{
			jsonToken = static_cast<APT::CJsonStringToken*>(jsonScanner.nextToken());
		}
		catch(const string& errorMessage)
		{
			cout<< errorMessage;
		}
		if( NULL == jsonToken)
		{
			break; // leave the while loop
		}
		switch(actual_state)
		{
		case WaitingForFirstToken:
			if(APT::CJsonToken::BEGIN_OBJECT == jsonToken->getType())
			{
				actual_state = WaitingForDbName;
			}
			else{
				actual_state = IsInErrorState;
			}
			break;
		case WaitingForDbName:
			if( 0 == jsonToken->getValue().compare("waypoints"))
			{
				WPDB_read = true;
				POIDB_read = false;
				actual_state = WaitingForNameSeparator;
			}
			else if( 0 == jsonToken->getValue().compare("pois"))
			{
				WPDB_read = false;
				POIDB_read = true;
				actual_state = WaitingForNameSeparator;
			}
			else{
				cout<<"Unkown Database"<<endl;
				return false;
			}
			break;
		case WaitingForAttributeName:
			if(APT::CJsonToken::BEGIN_OBJECT == jsonToken->getType())
			{

			}
			else if(APT::CJsonToken::STRING == jsonToken->getType())
			{
				lastReadAttributeName = jsonToken->getValue();
				actual_state = WaitingForNameSeparator;
				attributeNameRead = true;
			}
			break;
		case WaitingForNameSeparator:
			if(APT::CJsonToken::NAME_SEPARATOR == jsonToken->getType() && false == attributeNameRead)
			{
				actual_state = WaitingForArrayBegin;
			}
			else if(APT::CJsonToken::NAME_SEPARATOR == jsonToken->getType() && true == attributeNameRead)
			{
				actual_state = WaitingForValue;
			}

			break;
		case WaitingForValue:
			if(APT::CJsonToken::STRING == jsonToken->getType() || APT::CJsonToken::NUMBER == jsonToken->getType())
			{
				std::string stringAttributeValue;
				stringAttributeValue = jsonToken->str();
				stringAttributeValue = this->parseStringToken(stringAttributeValue);
				if(	WPDB_read == true && POIDB_read == false )
				{
					storeAttribute(&poi, lastReadAttributeName,  stringAttributeValue, storeOperationCounter);
				}
				else if(WPDB_read == false && POIDB_read == true )
				{
					storeAttribute(&poi, lastReadAttributeName,  stringAttributeValue, storeOperationCounter);
				}
				else
				{
					cout<<"Undefined state"<<endl;
				}
			}
			actual_state = WaitingForValueSeparator;
			break;
		case WaitingForArrayBegin:
			if(APT::CJsonToken::BEGIN_ARRAY == jsonToken->getType())
			{
				actual_state = WaitingForAttributeName;
			}

			break;
		case WaitingForValueSeparator:
			if(APT::CJsonToken::VALUE_SEPARATOR == jsonToken->getType())
			{
				if(!endArraySymbolRead)
				{
					actual_state = WaitingForAttributeName;
					attributeNameRead = false;
				}
				else{
					actual_state = WaitingForDbName;
					endArraySymbolRead = false;
					attributeNameRead = false;
				}
			}
			else if(APT::CJsonToken::END_OBJECT == jsonToken->getType() && false == endArraySymbolRead)
			{
				if(	WPDB_read == true && POIDB_read == false )
				{
					if( WP_NUMBER_OF_ATTRIBUTS == storeOperationCounter)
					{
						waypointDb.addWaypoint(dynamic_cast<CWaypoint &>(poi));
						//waypointDb.print();
					}
					else{
						cout<<"Could not add the waypoint to the database"<<endl;
						cout<<"Please check the fields above line "<<jsonScanner.scannedLine()<<endl;
						if( WP_NUMBER_OF_ATTRIBUTS < storeOperationCounter)
						{
							cout<<"Too many arguments have been parsed "<<endl;
						}
						else{
							cout<<"Too few arguments have been parsed "<<endl;
						}
					}
				}
				else if(WPDB_read == false && POIDB_read == true )
				{
					if(POI_NUMBER_OF_ATTRIBUTS == storeOperationCounter)
					{
						poiDb.addPOI(poi);
					}
					else{
						cout<<"Could not add the POI to the database"<<endl;
						cout<<"Please check the fields above line "<<jsonScanner.scannedLine()<<endl;
						if( POI_NUMBER_OF_ATTRIBUTS < storeOperationCounter)
						{
							cout<<"Too many arguments have been parsed "<<endl;
						}
						else{
							cout<<"Too few arguments have been parsed "<<endl;
						}
					}

				}
				else{
					cout<<"Configuration not allowed"<<endl;
				}
				actual_state = WaitingForValueSeparator;
				storeOperationCounter = 0;
			}
			else if(APT::CJsonToken::END_OBJECT == jsonToken->getType() && true == endArraySymbolRead)
			{

			}
			else if(APT::CJsonToken::END_ARRAY == jsonToken->getType())
			{
				endArraySymbolRead = true;
			}
			break;
		case IsInErrorState:
			cout<<"unexpected error occurred"<<endl;
			return false;
			break;
		default:
			break;
		}

	}
	return true;
}



CJsonPersistence::~CJsonPersistence() {
}

void storeAttribute(CPOI* poi, std::string attributeType,  std::string value, int& storeOperationCounter)
{
	if(0 == attributeType.compare("name"))
	{
		poi->setName(value);
		++storeOperationCounter;
	}
	else if(0 == attributeType.compare("latitude"))
	{
		poi->setLatitude(atof(value.c_str()));
		++storeOperationCounter;
	}
	else if(0 == attributeType.compare("longitude"))
	{
		poi->setLongitude(atof(value.c_str()));
		++storeOperationCounter;
	}

	else if(0 == attributeType.compare("type"))
	{
		poi->setType(string2Type(value));
		++storeOperationCounter;
	}
	else if(0 == attributeType.compare("description"))
	{
		poi->setDescription(value);
		++storeOperationCounter;
	}
	else{
		std::cout<<"Unknown attribute read"<<std::endl;
	}
}

std::string CJsonPersistence::parseStringToken(std::string stringToken) {
	size_t pos=string::npos;
	pos = stringToken.find_first_of(":");
	return stringToken.substr(pos+2, stringToken.size());
}
