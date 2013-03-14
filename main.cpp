// Copyright (c) 2013, Kirk Shoop (kirk.shoop@gmail.com)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, 
//  are permitted provided that the following conditions are met:
//
//  - Redistributions of source code must retain the above copyright notice, 
//      this list of conditions and the following disclaimer.
//  - Redistributions in binary form must reproduce the above copyright notice, 
//      this list of conditions and the following disclaimer in the documentation 
//      and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE 
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "rss.hpp"
#include "atom.hpp"
#include <network/http/client.hpp>
#include <boost/foreach.hpp>
#include <iostream>
#include <fstream>
#include <tuple>
#include <regex>
#include "rapidxml/rapidxml.hpp"
#include "cpprx/rx.hpp"
#include "cpplinq/linq.hpp"

namespace http = network::http;
namespace rss = network::rss;
namespace atom = network::atom;


struct Item
{
  struct Source 
  {
    std::string uri;
    std::string id;
    std::string title;
    std::string subtitle;
    std::string updated;
    std::string authorName;
    std::string authorEmail;
  } source;
  struct Data
  {
    std::string id;
    std::string title;
    std::string author;
    std::string published;
    std::string updated;
    std::string summary;
    std::string content;
  } data;
};

Item make_item(const http::client::response& r, const atom::feed& f, const atom::entry& e)
{
  Item result;
  r.get_source(result.source.uri);
  result.source.id = f.id();
  result.source.title = f.title();
  result.source.subtitle = f.subtitle();
  result.source.updated = f.updated();
  result.source.authorName = f.author().name();
  result.source.authorEmail = f.author().email();
  result.data.id = e.id();
  //result.data.author = ;
  result.data.title = e.title();
  result.data.published = e.published();
  result.data.updated = e.updated();
  result.data.summary = e.summary();
  result.data.content = e.content();
  return result;
}

Item make_item(const http::client::response& r, const rss::channel& c, const rss::item& i)
{
  Item result;
  r.get_source(result.source.uri);
  //result.source.id = ;
  result.source.title = c.title();
  result.source.subtitle = c.link();
  //result.source.updated = c.updated();
  result.source.authorName = i.author();
  //result.source.authorEmail = c.author().email();
  //result.data.id = ;
  result.data.author = i.author();
  result.data.title = i.title();
  //result.data.published = i.published();
  //result.data.updated = i.updated();
  //result.data.summary = i.summary();
  result.data.content = i.description();
  return result;
}

typedef std::shared_ptr<rxcpp::Observable<http::client::response>> HttpResponses;
HttpResponses HttpGet(
    const std::shared_ptr<rxcpp::Observable<std::string>>& sourceUris)
{
    return rxcpp::CreateObservable<http::client::response>(
        [=](std::shared_ptr<rxcpp::Observer<http::client::response>> observer) 
        -> rxcpp::Disposable
        {
            struct State 
            {
                State() : cancel(false) {}
                bool cancel;
                http::client client;
            };
            auto state = std::make_shared<State>();

            rxcpp::ComposableDisposable cd;

            cd.Add(rxcpp::Disposable([=]{ state->cancel = true; }));

            cd.Add(rxcpp::Subscribe(
                sourceUris,
            // on next
                [=](const std::string& uri)
                {
                    try {
                        http::client::request request(uri);
                        request << network::header("Connection", "close");
                        http::client::response response = state->client.get(request);
                        if (!state->cancel)
                            observer->OnNext(std::move(response));
                    } catch (...) {
                        observer->OnError(std::current_exception());
                    }
                },
            // on completed
                [=]
                {
                    if (!state->cancel)
                        observer->OnCompleted();
                },
            // on error
                [=](const std::exception_ptr& error)
                {
                    if (!state->cancel)
                        observer->OnError(error);
                }));
            return cd;
        }
    );
}


typedef std::shared_ptr<rapidxml::xml_document<>> shared_xmldoc;
typedef std::tuple<http::client::response, shared_xmldoc> XmlDoc;
typedef std::tuple<http::client::response, shared_xmldoc, rss::channel> RssChannel;
typedef std::tuple<http::client::response, shared_xmldoc, atom::feed> AtomFeed;


std::shared_ptr<rxcpp::Observable<XmlDoc>> XmlParse(
    const HttpResponses& responses)
{
    return rxcpp::CreateObservable<XmlDoc>(
        [=](std::shared_ptr<rxcpp::Observer<XmlDoc>> observer) 
        -> rxcpp::Disposable
        {
            struct State 
            {
                State() : cancel(false) {}
                bool cancel;
            };
            auto state = std::make_shared<State>();

            rxcpp::ComposableDisposable cd;

            cd.Add(rxcpp::Disposable([=]{ state->cancel = true; }));

            cd.Add(rxcpp::Subscribe(
                responses,
            // on next
                [=](const http::client::response& response)
                {
                    try {
                        auto doc = std::make_shared<rapidxml::xml_document<>>();
                        std::string response_body = body(response);
                        doc->parse<0>(const_cast<char*>(response_body.c_str()));
                        if (!state->cancel)
                            observer->OnNext(XmlDoc(response, std::move(doc))); 
                    } catch (...) {
                        observer->OnError(std::current_exception());
                    }
                },
            // on completed
                [=]
                {
                    if (!state->cancel)
                        observer->OnCompleted(); 
                },
            // on error
                [=](const std::exception_ptr& error)
                {
                    if (!state->cancel)
                        observer->OnError(error);
                }));
            return cd;
        }
    );
}


std::shared_ptr<rxcpp::Observable<RssChannel>> RssParse(
    const std::shared_ptr<rxcpp::Observable<XmlDoc>>& responses)
{
    return rxcpp::CreateObservable<RssChannel>(
        [=](std::shared_ptr<rxcpp::Observer<RssChannel>> observer) 
        -> rxcpp::Disposable
        {
            struct State 
            {
                State() : cancel(false) {}
                bool cancel;
            };
            auto state = std::make_shared<State>();

            rxcpp::ComposableDisposable cd;

            cd.Add(rxcpp::Disposable([=]{ state->cancel = true; }));

            cd.Add(rxcpp::Subscribe(
                responses,
            // on next
                [=](const XmlDoc& item)
                {
                    try {
                        auto response = std::get<0>(item);
                        auto doc = std::get<1>(item);
                        rss::channel channel(doc);
                        if (!state->cancel)
                            observer->OnNext(RssChannel(std::move(response), std::move(doc), std::move(channel))); 
                    } catch (...) {
                        observer->OnError(std::current_exception());
                    }
                },
            // on completed
                [=]
                {
                    if (!state->cancel)
                        observer->OnCompleted(); 
                },
            // on error
                [=](const std::exception_ptr& error)
                {
                    if (!state->cancel)
                        observer->OnError(error);
                }));
            return cd;
        }
    );
}

std::shared_ptr<rxcpp::Observable<AtomFeed>> AtomParse(
    const std::shared_ptr<rxcpp::Observable<XmlDoc>>& responses)
{
    return rxcpp::CreateObservable<AtomFeed>(
        [=](std::shared_ptr<rxcpp::Observer<AtomFeed>> observer) 
        -> rxcpp::Disposable
        {
            struct State 
            {
                State() : cancel(false) {}
                bool cancel;
            };
            auto state = std::make_shared<State>();

            rxcpp::ComposableDisposable cd;

            cd.Add(rxcpp::Disposable([=]{ state->cancel = true; }));

            cd.Add(rxcpp::Subscribe(
                responses,
            // on next
                [=](const XmlDoc& item)
                {
                    try {
                        auto response = std::get<0>(item);
                        auto doc = std::get<1>(item);
                        atom::feed feed(doc);
                        if (!state->cancel)
                            observer->OnNext(AtomFeed(std::move(response), std::move(doc), std::move(feed))); 
                    } catch (...) {
                        observer->OnError(std::current_exception());
                    }
                },
            // on completed
                [=]
                {
                    if (!state->cancel)
                        observer->OnCompleted(); 
                },
            // on error
                [=](const std::exception_ptr& error)
                {
                    if (!state->cancel)
                        observer->OnError(error);
                }));
            return cd;
        }
    );
}

std::shared_ptr<rxcpp::Observable<Item>> AtomEntries(
    const std::shared_ptr<rxcpp::Observable<AtomFeed>>& responses
)
{
    return rxcpp::CreateObservable<Item>(
        [=](std::shared_ptr<rxcpp::Observer<Item>> observer) 
        -> rxcpp::Disposable
        {
            struct State 
            {
                State() : cancel(false) {}
                bool cancel;
            };
            auto state = std::make_shared<State>();

            rxcpp::ComposableDisposable cd;

            cd.Add(rxcpp::Disposable([=]{ state->cancel = true; }));

            cd.Add(rxcpp::Subscribe(
                responses,
            // on next
                [=](const AtomFeed& item)
                {
                    try {
                        if(state->cancel) return ;
                        auto& feed = std::get<2>(item);
                        for (auto& entry : feed) {
                          observer->OnNext(make_item(
                            std::get<0>(item),
                            feed,
                            entry));
                        }
                    } catch (...) {
                        observer->OnError(std::current_exception());
                    }
                },
            // on completed
                [=]
                {
                    if (!state->cancel)
                        observer->OnCompleted(); 
                },
            // on error
                [=](const std::exception_ptr& error)
                {
                    if (!state->cancel)
                        observer->OnError(error);
                }));
            return cd;
        }
    );
}


std::shared_ptr<rxcpp::Observable<Item>> RssEntries(
    const std::shared_ptr<rxcpp::Observable<RssChannel>>& responses)
{
    return rxcpp::CreateObservable<Item>(
        [=](std::shared_ptr<rxcpp::Observer<Item>> observer) 
        -> rxcpp::Disposable
        {
            struct State 
            {
                State() : cancel(false) {}
                bool cancel;
            };
            auto state = std::make_shared<State>();

            rxcpp::ComposableDisposable cd;

            cd.Add(rxcpp::Disposable([=]{ state->cancel = true; }));

            cd.Add(rxcpp::Subscribe(
                responses,
            // on next
                [=](const RssChannel& item)
                {
                    try {
                        if(state->cancel) return ;
                        auto& channel = std::get<2>(item);
                        for (auto& entry : channel) {
                          observer->OnNext(make_item(
                            std::get<0>(item),
                            channel,
                            entry));
                        }
                    } catch (...) {
                        observer->OnError(std::current_exception());
                    }
                },
            // on completed
                [=]
                {
                    if (!state->cancel)
                        observer->OnCompleted(); 
                },
            // on error
                [=](const std::exception_ptr& error)
                {
                    if (!state->cancel)
                        observer->OnError(error);
                }));
            return cd;
        }
    );
}

namespace News {

struct http_get {};
template<class... Arg>
auto rxcpp_chain(http_get&&, Arg&& ...arg)
-> decltype(HttpGet(std::forward<Arg>(arg)...)) {
    return HttpGet(std::forward<Arg>(arg)...);
}

struct xml_parse {};
template<class... Arg>
std::shared_ptr<rxcpp::Observable<XmlDoc>> 
rxcpp_chain(xml_parse&&, Arg&& ...arg) 
{
  return XmlParse(std::forward<Arg>(arg)...);
}

struct rss_parse {};
template<class... Arg>
auto rxcpp_chain(rss_parse&&, Arg&& ...arg) 
  -> decltype(RssParse(std::forward<Arg>(arg)...)) {
  return RssParse(std::forward<Arg>(arg)...);
}

struct atom_parse {};
template<class... Arg>
auto rxcpp_chain(atom_parse&&, Arg&& ...arg) 
  -> decltype(AtomParse(std::forward<Arg>(arg)...)) {
  return AtomParse(std::forward<Arg>(arg)...);
}

struct atom_entries {};
template<class... Arg>
auto rxcpp_chain(atom_entries&&, Arg&& ...arg) 
  -> decltype(AtomEntries(std::forward<Arg>(arg)...)) {
  return AtomEntries(std::forward<Arg>(arg)...);
}

struct rss_entries {};
template<class... Arg>
auto rxcpp_chain(rss_entries&&, Arg&& ...arg) 
  -> decltype(RssEntries(std::forward<Arg>(arg)...)) {
  return RssEntries(std::forward<Arg>(arg)...);
}

}

std::regex content_type_regex("^([a-z]+)[/]([a-z]+)(?:\\+([a-z]+))?(?:;\\s*charset=([a-z0-9\\-]+))?");

struct ContentType
{
  std::string top;
  std::string sub;
  std::string format;
};

ContentType extract_content_type(const std::string& input)
{
    const std::sregex_iterator end;
    for (std::sregex_iterator i(input.cbegin(), input.cend(), content_type_regex);
        i != end;
        ++i)
    {
        ContentType result;
        result.top = (*i)[1];
        result.sub = (*i)[2];
        result.format = (*i)[3];
        return result;
    }
    throw std::range_error("search key not found");
}

int main(int argc, char* argv[]) {

  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " <url>..." << std::endl;
    return 1;
  }

  try {
    auto newthread = std::make_shared<rxcpp::NewThreadScheduler>();
    auto output = std::make_shared<rxcpp::EventLoopScheduler>();
    auto currentthread = std::make_shared<rxcpp::CurrentThreadScheduler>();

    auto uris = rxcpp::CreateSubject<std::string>();

    // get docs via http
    auto responsesByContentType = from(uris)
      .observe_on(newthread)
      .chain<News::http_get>()
      .group_by([](const http::client::response& response){
        std::string contentType;
        response.get_headers(
          "Content-Type", 
          [&](std::string const& name, std::string const& value){
            contentType = value;});
        return contentType;}
      );

    // parse xml docs
    auto xmlDocsByRoot = from(responsesByContentType)
      .where([](const std::shared_ptr<rxcpp::GroupedObservable<std::string, http::client::response>>& grsp){
        auto contentTypeField = grsp->Key();
        auto contentType = extract_content_type(contentTypeField);
        if ((contentType.top == "application" || contentType.top == "text") &&
          (!contentType.format.empty() ? contentType.format == "xml": contentType.sub == "xml")) {
          return true;
        }
        return false;}
      )
      .select_many()
      .observe_on(newthread)
      .chain<News::xml_parse>()
      .group_by([](const XmlDoc& doc){
        std::string name;
        auto docNode = std::get<1>(doc)->first_node();
        if (docNode) {
          name = std::get<1>(doc)->first_node()->name();
        }
        return name;
      });

    std::exception_ptr error;
    rxcpp::ComposableDisposable cd;
      
    rxcpp::SharedDisposable sd;
    cd.Add(sd);

    // parse and print atom feeds
    from(xmlDocsByRoot)
      .where([](const std::shared_ptr<rxcpp::GroupedObservable<std::string, XmlDoc>>& grsp){
        auto rootNode = grsp->Key();
        return rootNode == "feed";}
      )
      .select_many()
      .observe_on(newthread)
      .chain<News::atom_parse>()
      .chain<News::atom_entries>()
      .observe_on(output)
      .subscribe([=](const Item& i){
          std::cout << "atom: (" << i.source.title << ") " << i.data.title << std::endl;},
          [](){},
          [&](const std::exception_ptr& e){
              error = e; cd.Dispose(); uris->OnError(e);}
      );

    // parse and print rss feeds
    from(xmlDocsByRoot)
      .where([](const std::shared_ptr<rxcpp::GroupedObservable<std::string, XmlDoc>>& grsp){
        auto rootNode = grsp->Key();
        return rootNode == "rss";}
      )
      .select_many()
      .observe_on(newthread)
      .chain<News::rss_parse>()
      .chain<News::rss_entries>()
      .observe_on(output)
      .subscribe([=](const Item& i){
          std::cout << "rss : (" << i.source.title << ") " << i.data.title << std::endl;},
          [](){},
          [&](const std::exception_ptr& e){
              error = e; cd.Dispose(); uris->OnError(e);}
      );


      // exit in 15 seconds
      cd.Add(output->Schedule(
          std::chrono::seconds(15),
          [=](rxcpp::Scheduler::shared) {
              std::cout << "your 15 seconds of fame are up!" << std::endl;
              cd.Dispose();
              uris->OnCompleted();
              return rxcpp::Disposable::Empty();
          }));

      // send in the uris every 5 seconds
      sd.Set(output->Schedule(
         rxcpp::fix0([=](
             rxcpp::Scheduler::shared s,
             std::function<rxcpp::Disposable(rxcpp::Scheduler::shared)> self)
         -> rxcpp::Disposable
         {
             try {
                 for (int cursor = 1; cursor < argc; ++cursor){
                     uris->OnNext(argv[cursor]);
                 }
                 sd.Set(s->Schedule(
                    std::chrono::seconds(5), 
                    std::move(self)));
             } catch (...) {
                 uris->OnError(std::current_exception());
             }   
             return rxcpp::Disposable::Empty();             
         })));

      // wait until time to exit 
      from(uris).for_each([=](const std::string& i){});
      if (error) {
          std::rethrow_exception(error);}
  }
  catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  std::cout << "exiting" << std::endl;

  return 0;
}
