// Copyright (c) 2013, Kenton Varda <temporal@gmail.com>
// Copyright (c) 2014, Jakub Spiewak <j.m.spiewak@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Run like:
//   ./addressbook write | ./addressbook read

#include <addressbook.capnp.h>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <iostream>

using addressbook::Person;
using addressbook::AddressBook;

void writeAddressBook(int fd) {
  ::capnp::MallocMessageBuilder message;

  AddressBook::Builder addressBook = message.initRoot<AddressBook>();
  addressBook.people.init(2);

  Person::Builder alice = addressBook.people[0];
  alice.id = 123;
  alice.name = "Alice";
  alice.email = "alice@example.com";
  alice.phones.init(1);
  alice.phones[0].number = "555-1212";
  alice.phones[0].type = Person::PhoneNumber::Type::MOBILE;
  alice.employment.school = "MIT";

  Person::Builder bob = addressBook.people[1];
  bob.id = 456;
  bob.name = "Bob";
  bob.email = "bob@example.com";
  auto bobPhones = bob.phones.init(2);
  bobPhones[0].number = "555-4567";
  bobPhones[0].type = Person::PhoneNumber::Type::HOME;
  bobPhones[1].number = "555-7654";
  bobPhones[1].type = Person::PhoneNumber::Type::WORK;
  bob.employment.unemployed = ::capnp::VOID;

  writePackedMessageToFd(fd, message);
}

void printAddressBook(int fd) {
  ::capnp::PackedFdMessageReader message(fd);

  AddressBook::Reader addressBook = message.getRoot<AddressBook>();

  for (Person::Reader person : addressBook.people) {
    std::cout << person.name.cStr() << ": "
              << person.email.cStr() << std::endl;

    for (Person::PhoneNumber::Reader phone: person.phones) {
      const char* typeName = "UNKNOWN";
      switch (phone.type) {
        case Person::PhoneNumber::Type::MOBILE: typeName = "mobile"; break;
        case Person::PhoneNumber::Type::HOME: typeName = "home"; break;
        case Person::PhoneNumber::Type::WORK: typeName = "work"; break;
      }

      std::cout << "  " << typeName << " phone: "
                << phone.number.cStr() << std::endl;
    }

    Person::Employment::Reader employment = person.employment;
    switch (employment.which()) {
      case Person::Employment::UNEMPLOYED:
        std::cout << "  unemployed" << std::endl;
        break;
      case Person::Employment::EMPLOYER:
        std::cout << "  employer: "
                  << employment.employer.cStr() << std::endl;
        break;
      case Person::Employment::SCHOOL:
        std::cout << "  student at: "
                  << employment.school.cStr() << std::endl;
        break;
      case Person::Employment::SELF_EMPLOYED:
        std::cout << "  self-employed" << std::endl;
        break;
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Missing arg." << std::endl;
    return 1;
  } else if (strcmp(argv[1], "write") == 0) {
    writeAddressBook(1);
  } else if (strcmp(argv[1], "read") == 0) {
    printAddressBook(0);
  } else {
    std::cerr << "Invalid arg: " << argv[1] << std::endl;
    return 1;
  }

  return 0;
}

